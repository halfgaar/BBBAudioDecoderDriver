/*
 * Copyright (C) 2018  Wiebe Cazemier <wiebe@halfgaar.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * https://www.gnu.org/licenses/gpl-2.0.html
 */

#include <QMutexLocker>

#include "audioringbuffer.h"
#include <iostream>

int readFromCircularBuffer(void *opaque, uint8_t *buf, int buf_size)
{
    PlaybackWorker *a = (PlaybackWorker*)opaque;

    // If you don't do this, ffmpeg will keep reading PCM and waiting until it sees valid codec frames again.
    if (!a->mRingBuffer.DIR9001SeesEncodedAudio())
        return -1;

    int bytesRead = a->mRingBuffer.circularBufferToDecodeBuffer(buf, buf_size);
    return bytesRead;
}

CaptureWorker::CaptureWorker(AudioRingBuffer &ringBuffer) :
    mRingBuffer(ringBuffer)
{

}

AudioRingBuffer::AudioRingBuffer(GpIOFunctions &gpIOFunctions, QObject *parent) : QObject(parent),
    mGpPIOFunctions(gpIOFunctions),
    capture_handle(NULL),
    playback_handle(NULL),
    mCaptureWorker(*this),
    mCaptureThread(),
    buffer(new char[RING_BUFFER_SIZE]),
    freeBytes(RING_BUFFER_SIZE),
    usedBytes(),
    captureFrameSize(snd_pcm_format_width(SND_PCM_FORMAT_S16_LE) / 8 * 2),
    mPlaybackWorker(NULL),
    mPlaybackThread(new QThread())
{
    mPlaybackThread.setObjectName("Decode/Playback");
    mCaptureThread.setObjectName("Capture");
    mPlaybackThread.start();
    mCaptureThread.start();

    avcodec_register_all();
    av_register_all();

    mCaptureBuffer = malloc(FRAMES_IN_BUFFER * captureFrameSize);

    makePlaybackWorker();

    initCaptureDevice();
    mCaptureWorker.moveToThread(&mCaptureThread);

    // This line, weirdly enough, causes the debugger to say SIGILL on every statement I break,
    // but the code itself seems to work.
    connect(&mGpPIOFunctions, &GpIOFunctions::signalAudioFormatChanged, this, &AudioRingBuffer::onAudioFormatChanged);

    printStatusTimer.setInterval(1000);
    connect(&printStatusTimer, &QTimer::timeout, this, &AudioRingBuffer::onStatusTimer);
    printStatusTimer.start();
}

AudioRingBuffer::~AudioRingBuffer()
{
    free(mCaptureBuffer);
    delete[] buffer;

    if (mPlaybackWorker)
        mPlaybackWorker->deleteLater();
}

void AudioRingBuffer::captureBufferToCircularBuffer(int bytes)
{
    char * captureBuffer = (char*)mCaptureBuffer;
    freeBytes.acquire(bytes);
    for (int i = 0; i < bytes; ++i)
    {

        buffer[indexProducer++ % RING_BUFFER_SIZE] = captureBuffer[i];

    }
    usedBytes.release(bytes);
}

void AudioRingBuffer::onStatusTimer()
{
    QString line = QString("Buf size: %1").arg(usedBytes.available());
    std::cout << line.toLatin1().data() << std::endl;
    emit bufferBytesInfo(line);
}

void AudioRingBuffer::onDecodingAborted()
{
    std::cout << "Decoding loop stopped." << std::endl;
    makePlaybackWorker();
    QMetaObject::invokeMethod(mPlaybackWorker, "doWork");
}

void AudioRingBuffer::onAudioFormatChanged(bool encoded)
{
    Q_UNUSED(encoded)
    std::cout << "Aborting, because we got signal AudioFormatChanged." << std::endl;

    if (mPlaybackWorker)
    {
        mPlaybackWorker->mThisThreadAbort = true;
    }
}

void AudioRingBuffer::makePlaybackWorker()
{
    if (mPlaybackWorker)
    {
        mPlaybackWorker = 0;
    }

    closePlaybackDevice();
    mPlaybackWorker = new PlaybackWorker(*this);
    mPlaybackWorker->moveToThread(&mPlaybackThread);
    connect(mPlaybackWorker, &PlaybackWorker::signalDecodingAborted, this, &AudioRingBuffer::onDecodingAborted);
    connect(mPlaybackWorker, &PlaybackWorker::signalDecodingAborted, mPlaybackWorker, &PlaybackWorker::deleteLater);
    connect(mPlaybackWorker, &PlaybackWorker::newCodecName, this, &AudioRingBuffer::newCodecName);
}

int AudioRingBuffer::circularBufferToDecodeBuffer(uint8_t * buf, int nbytes)
{
    //if (bytesStored >= 2048)
    //{
        //QString e = QString("We have %1 bytes in the buffer. This is >= 2048. We're getting too far behind.").arg(bytesStored);
        //std::cerr << e.toLatin1().data() << std::endl;
    //}

    usedBytes.acquire(nbytes);
    for (int i = 0; i < nbytes; ++i)
    {
        buf[i] = buffer[indexConsumer++ % RING_BUFFER_SIZE];
    }
    freeBytes.release(nbytes);

    return nbytes;
}

void CaptureWorker::doWork()
{
    while (true)
    {
        const int framesFreeInBuffer = (mRingBuffer.freeBytes.available()) / mRingBuffer.captureFrameSize;
        const int framesToRead = std::min(framesFreeInBuffer, FRAMES_IN_BUFFER);

        if (framesToRead == 0)
        {
            std::cerr << "Buffer full. What to do?" << std::endl;
            continue;
        }
        else
        {
            int noOfFramesRread = snd_pcm_readi(mRingBuffer.capture_handle, mRingBuffer.mCaptureBuffer, framesToRead);

            if (noOfFramesRread > 0)
            {
                //std::cout << "Feeding no of frames: " << noOfFrames << std::endl;
                mRingBuffer.captureBufferToCircularBuffer(noOfFramesRread * mRingBuffer.captureFrameSize);
            }
            else if (noOfFramesRread == -EPIPE)
            {
                std::cerr << "Broken read pipe; an overrun occurred. Re-preparing PCM" << std::endl;
                snd_pcm_prepare(mRingBuffer.capture_handle);
            }
            else if (noOfFramesRread == -ESTRPIPE)
            {
                std::cerr << "a suspend event occurred (stream is suspended and waiting for an application recovery)" << std::endl;
            }
            else
            {
                std::cerr << "Unkown error code in capture thread: " << noOfFramesRread << std::endl;
            }
        }
    }
}

void AudioRingBuffer::initCaptureDevice()
{
    unsigned int rate = 48000; // Actually unnecessary, because my hacked mcasp davinci driver ignores it, because it's clocked externally.
    snd_pcm_hw_params_t *hw_params;

    checkError(snd_pcm_hw_params_malloc(&hw_params));
    checkError(snd_pcm_open(&capture_handle, "hw:0", SND_PCM_STREAM_CAPTURE, 0));
    checkError(snd_pcm_hw_params_any(capture_handle, hw_params));
    checkError(snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED));
    checkError(snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S16_LE));
    checkError(snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &rate, 0));
    checkError(snd_pcm_hw_params_set_channels(capture_handle, hw_params, 2));
    checkError(snd_pcm_hw_params_set_buffer_time(capture_handle, hw_params, 10000, 0)); // low latency
    //uint buffer_time; snd_pcm_hw_params_get_buffer_time_max(hw_params, &buffer_time, 0);
    //std::cerr << buffer_time << std::endl;
    checkError(snd_pcm_hw_params(capture_handle, hw_params));
    checkError(snd_pcm_prepare(capture_handle));

    snd_pcm_hw_params_free(hw_params);
}

void AudioRingBuffer::openPlaybackDevice(int numberOfChannels)
{
    unsigned int rate = 48000; // Actually unnecessary, because my hacked mcasp davinci driver ignores it, because it's clocked externally.
    snd_pcm_hw_params_t *hw_params;

    checkError(snd_pcm_hw_params_malloc(&hw_params));
    checkError(snd_pcm_open(&playback_handle, "hw:0", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK));
    checkError(snd_pcm_hw_params_any(playback_handle, hw_params));
    checkError(snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED));
    checkError(snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_S16_LE));
    checkError(snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rate, 0));
    checkError(snd_pcm_hw_params_set_channels(playback_handle, hw_params, numberOfChannels));
    checkError(snd_pcm_hw_params(playback_handle, hw_params));
    checkError(snd_pcm_prepare(playback_handle));

    snd_pcm_hw_params_free(hw_params);
}

void AudioRingBuffer::closePlaybackDevice()
{
    if (playback_handle)
    {
        checkError(snd_pcm_close(playback_handle));
        playback_handle = NULL;
    }
}

void AudioRingBuffer::checkError(int ret)
{
    if (ret < 0)
        std::cerr << "Something went wrong initing the audio device and I'm too lazy to figure out what" << std::endl;
}

void AudioRingBuffer::startThreads()
{
    QMetaObject::invokeMethod(&mCaptureWorker, "doWork");
    QMetaObject::invokeMethod(mPlaybackWorker, "doWork");
}


PlaybackWorker::PlaybackWorker(AudioRingBuffer &ringBuffer) :
    context(0),
    frame(av_frame_alloc()),
    swr_ctx(swr_alloc()),
    avFormatContext(avformat_alloc_context()),
    avIO_ctx_buffer((uint8_t*)av_malloc(AVIO_CTX_BUFFER_SIZE)),
    avIOContext(avio_alloc_context(avIO_ctx_buffer, AVIO_CTX_BUFFER_SIZE, 0, this, readFromCircularBuffer, NULL, NULL)),
    mRingBuffer(ringBuffer)
{
    avFormatContext->pb = avIOContext; // I need to create the AVFormatContext manually and assign pb because I'm using my own IO system.
    av_init_packet(&pkt);
}

PlaybackWorker::~PlaybackWorker()
{
    if (mAVInputOpened)
        avformat_close_input(&avFormatContext);
    av_freep(&avIOContext->buffer); // taking the buffer to free from inside the AVIOContext, because it might have been changed by avformat and not be avIO_ctx_buffer
    av_freep(&avIOContext); // The docs say to use the non-existing function avio_context_free, but http://ffmpeg.org/doxygen/trunk/avio_reading_8c-example.html shows to use av_freep()

    if (avFormatContext)
        avformat_free_context(avFormatContext);

    swr_free(&swr_ctx);
    av_frame_free(&frame);
    avcodec_free_context(&context);

    if (converted_samples)
    {
        av_freep(&converted_samples[0]);
        av_freep(&converted_samples);
    }
    std::cerr << "Last line of ~PlaybackWorker" << std::endl;
}

void PlaybackWorker::doWork()
{
    if (mRingBuffer.mGpPIOFunctions.DIR9001SeesEncodedAudio())
    {
        decodeWithFFMpeg();
    }
    else
    {
        writeDirectlyToOutput();
    }
}

void PlaybackWorker::decodeWithFFMpeg()
{
    emit newCodecName("Detecting codec...");
    avFormatContext->probesize = 4096; // increase speed of codec detection with avformat_find_stream_info() below.

    AVInputFormat *spdif = av_find_input_format("spdif");
    int ret = avformat_open_input(&avFormatContext, NULL, spdif, NULL);

    if (ret < 0)
    {
        std::cerr << "Error opening stream for playback" << std::endl;

        char ffmpegError[255];
        av_strerror(ret, ffmpegError, 255);
        std::cerr << ffmpegError << std::endl;

        emit signalDecodingAborted();
        return;
    }

    mAVInputOpened = true;

    ret = avformat_find_stream_info(avFormatContext, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Could not find stream information\n");
    }
    av_dump_format(avFormatContext, 0, NULL, 0);

    AVCodec *codec;
    int stream = av_find_best_stream(avFormatContext, AVMediaType::AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);

    if (stream < 0)
    {
        std::cerr << "No stream found." << std::endl;
        emit signalDecodingAborted();
        return;
    }
    AVStream *st = avFormatContext->streams[stream];

    context = avcodec_alloc_context3(codec);
    av_opt_set_double(context, "drc_scale", 0, AV_OPT_SEARCH_CHILDREN); // Want to hear the long story? E-mail me.
    avcodec_parameters_to_context(context, st->codecpar);
    avcodec_open2(context, codec, NULL);

    av_opt_set_channel_layout(swr_ctx, "in_channel_layout", context->channel_layout, 0);
    av_opt_set_channel_layout(swr_ctx, "out_channel_layout", AV_CH_LAYOUT_7POINT1, 0); // To match the hardware. I hope ffmpeg will always upmix by adding silent channels when they're not in the source.
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", context->sample_fmt, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    if ((ret = swr_init(swr_ctx)) < 0)
    {
        std::cerr << "Failed to initialize the resampling context, error :" << ret << std::endl;
        char bla[255];
        av_strerror(ret, bla, 255);
        std::cerr << bla << std::endl;

        emit signalDecodingAborted();
        return;
    }

    // AC3 results in AV_SAMPLE_FMT_FLTP (8), 4 bytes per sample

    // Allocating more sample space than necessary. I will never change the number of samples, and so I can prevent having to allocate in the loop.
    ret = av_samples_alloc_array_and_samples(&converted_samples, NULL, context->channels, 65536, AV_SAMPLE_FMT_S16, 0);

    if (ret  < 0)
    {
        char ffmpegError[255];
        av_strerror(ret, ffmpegError, 255);
        std::cerr << ffmpegError << std::endl;
    }

    const AVCodecDescriptor *descriptor = avcodec_descriptor_get(context->codec_id);
    const QString name = QString("%1 [%2 channels]").arg(descriptor->name).arg(context->channels);
    emit newCodecName(name);
    std::cout << name.toLatin1().data() << std::endl;

    mRingBuffer.openPlaybackDevice(8);

    bool initialPileUpSkipped = false;

    uint64_t previousChannelLayout = context->channel_layout;
    int previousChannels = context->channels;
    AVCodecID previousCodecID = context->codec_id;


    while (!mThisThreadAbort)
    {
        // This keeps reading data but only return once a complete frame is present. So for example when paused and receiving zeroes,
        // this statement hangs. Note: some devices send zeroes when paused, other stop sending encoded audio, and the DIR9001 will report
        // raw PCM again.
        ret = av_read_frame(avFormatContext, &pkt);

        // When we have received an abort, this frame is likely corrupt, because likely the audio format changed. Don't try to decode it.
        if (mThisThreadAbort)
            continue;

        if (ret < 0)
        {
            break;
        }

        if (!initialPileUpSkipped)
        {
            int usedBytes = mRingBuffer.usedBytes.available();
            if (usedBytes > 4096)
            {
                std::cerr << "Initial buffer pile-up too big: " << usedBytes << ". Not writing frame to output to catch up with input and prevent garble output" << std::endl;
                continue;
            }
            initialPileUpSkipped = true;
        }

        avcodec_send_packet(context, &pkt);
        avcodec_receive_frame(context, frame);
        av_packet_unref(&pkt);

        // I noticed that channel count and layout can change dynamically, after which swr_convert would crash. I'm not sure what dynamic changes I could
        // make work, so I just break, so that ffmpeg is reinitialized.
        if (previousChannelLayout != context->channel_layout || previousChannels != context->channels || previousCodecID != context->codec_id)
        {
            break;
        }

        ret = swr_convert(swr_ctx, converted_samples, frame->nb_samples, (const uint8_t**)frame->data, frame->nb_samples);

        if (ret < 0)
        {
            std::cerr << "Sample format conversion error: " << ret << std::endl;
            break;
        }

        previousChannelLayout = context->channel_layout;
        previousChannels = context->channels;
        previousCodecID = context->codec_id;

        // Because we converted from planar to interleaved, all samples are in the first element of the array.
        ret = snd_pcm_writei(mRingBuffer.playback_handle, converted_samples[0], frame->nb_samples);
        if (ret == -EPIPE)
        {
            std::cerr << "Broken write pipe because the ALSA playback buffer ran out. Re-preparing PCM" << std::endl;
            QThread::msleep(50);
            snd_pcm_prepare(mRingBuffer.playback_handle);
        }
        else if (ret < 0)
        {
            std::cerr << "Unknown ALSA snd_pcm_writei error: " << ret << std::endl;
        }
    }

    emit signalDecodingAborted();
}

void PlaybackWorker::writeDirectlyToOutput()
{
    emit newCodecName("Raw PCM");

    mRingBuffer.openPlaybackDevice(2);
    const uint totalBytes = FRAMES_IN_BUFFER * mRingBuffer.captureFrameSize;
    uint8_t buf[totalBytes];

    QThread::msleep(20);

    while (!mThisThreadAbort)
    {
        // Because of the semaphores, this hangs until enough frames are available.
        mRingBuffer.circularBufferToDecodeBuffer(buf, totalBytes);

        // Check if we are seeing encoded audio before writing to Alsa. This fixes getting garble audio on resume-after-pause on some hardware.
        if (mRingBuffer.mGpPIOFunctions.DIR9001SeesEncodedAudio())
        {
            break;
        }

        int ret = snd_pcm_writei(mRingBuffer.playback_handle, buf, FRAMES_IN_BUFFER);
        if (ret == -EPIPE)
        {
            std::cerr << "Broken write pipe because the ALSA playback buffer ran out. Re-preparing PCM" << std::endl;
            QThread::msleep(10);
            snd_pcm_prepare(mRingBuffer.playback_handle);
        }
        else if (ret < 0)
        {
            std::cerr << "Unknown ALSA snd_pcm_writei error: " << ret << std::endl;
        }
    }

    std::cout << "About to emit signal 'decodingAborted'" << std::endl;
    emit signalDecodingAborted();
}














