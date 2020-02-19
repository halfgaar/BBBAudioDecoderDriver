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

#ifndef AUDIORINGBUFFER_H
#define AUDIORINGBUFFER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QSemaphore>
#include <QTimer>
#include <QScopedPointer>
#include <alsa/asoundlib.h>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavformat/avio.h>
    #include <libswresample/swresample.h>
    #include <libavutil/samplefmt.h>
    #include <libavutil/opt.h>
}

#include "gpiofunctions.h"

#define FRAMES_IN_BUFFER 64
#define RING_BUFFER_SIZE 10485760
#define AVIO_CTX_BUFFER_SIZE 4096

#define MUTE_MODE_UNDEFINED 0
#define MUTE_MODE_UNMUTED 1
#define MUTE_MODE_MUTED 2

class AudioRingBuffer;

class CaptureWorker : public QObject
{
    Q_OBJECT

    AudioRingBuffer &mRingBuffer;

public:
    CaptureWorker(AudioRingBuffer &ringBuffer);

public slots:
    void doWork();

signals:
};

class PlaybackWorker : public QObject
{
    Q_OBJECT


    AVCodecContext *context = 0;
    AVFrame *frame;
    struct SwrContext *swr_ctx;
    uint8_t **converted_samples = 0;

    AVFormatContext *avFormatContext;
    uint8_t *avIO_ctx_buffer;
    AVIOContext *avIOContext;
    bool mAVInputOpened = false;

    AVPacket pkt;

public:
    PlaybackWorker(AudioRingBuffer &ringBuffer);
    ~PlaybackWorker();

    AudioRingBuffer &mRingBuffer;
    bool mThisThreadAbort = false; // This bool and the old system of signals to detect format change is deprecated and can actually be removed.

public slots:
    void doWork();
    void decodeWithFFMpeg();
    void writeDirectlyToOutput();

signals:
    void signalDecodingAborted();
    void newCodecName(const QString &name);
};

class AudioRingBuffer : public QObject
{
    Q_OBJECT

    GpIOFunctions &mGpPIOFunctions;

    friend class CaptureWorker;
    friend class PlaybackWorker;

    snd_pcm_t *capture_handle;
    snd_pcm_t *playback_handle;
    void *mCaptureBuffer; // For snd_pcm_readi to read into
    CaptureWorker mCaptureWorker;
    QThread mCaptureThread;

    char * buffer; // The circular FIFO buffer that connects everything together: TODO: ffmpeg also has a FIFO buffer, should I use that?
    quint32 indexProducer = 0;
    quint32 indexConsumer = 0;
    QSemaphore freeBytes;
    QSemaphore usedBytes;

    const int captureFrameSize;

    PlaybackWorker *mPlaybackWorker;
    QThread mPlaybackThread;

    QTimer printStatusTimer;

    QMutex sampleRateCounterMutex;
    QTimer sampeRateCalculatorTimer;
    uint byteCounter = 0;
    bool phaseLocked = false; // See onSampleRateCalculatorTimer()

    snd_mixer_t *mixer_handle = nullptr;
    const char *card = "default";
    QList<QString> selem_name;
    bool giveUpOnMixer = false;

    void initCaptureDevice();
    void openPlaybackDevice(int numberOfChannels);
    void setAlsaMute(bool mute);
    void closePlaybackDevice();
    void checkError(int ret);
    void checkMixerError(int ret);
    void makePlaybackWorker();
public:
    explicit AudioRingBuffer(GpIOFunctions &gpIOFunctions, QObject *parent = nullptr);
    ~AudioRingBuffer();

    int circularBufferToDecodeBuffer(uint8_t *buf, int nbytes);
    inline bool DIR9001SeesEncodedAudio() { return mGpPIOFunctions.DIR9001SeesEncodedAudio(); }
    void startThreads();

signals:
    void newCodecName(const QString &name);
    void bufferBytesInfo(const QString &line);

private slots:
    void captureBufferToCircularBuffer(int bytes);
    void onStatusTimer();
    void onDecodingAborted();
    void onAudioFormatChanged(bool encoded);
    void onSampleRateCalculatorTimer();

public slots:
};

#endif // AUDIORINGBUFFER_H
