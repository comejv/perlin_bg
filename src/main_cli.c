#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FNL_IMPL
#include "FastNoiseLite.h"
#define MSF_GIF_IMPL
#include "msf_gif.h"

// FFmpeg
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

// Getopt
extern char *optarg;

typedef enum
{
  OUT_GIF,
  OUT_WEBM
} OutFmt;

static void dual(float v, uint8_t *r, uint8_t *g, uint8_t *b)
{
  float fc = (cosf(v * (float) M_PI) + 1.0f) * 0.5f;
  float fs = (sinf(v * (float) M_PI) + 1.0f) * 0.5f;
  *r = (uint8_t) (fc * 255.0f);
  *g = (uint8_t) (v * 10.0f);
  *b = (uint8_t) (fs * 100.0f);
}

static void rainbow(float v, uint8_t *r, uint8_t *g, uint8_t *b)
{
  float a_r = 0.5f, ph_r = 0.0f;
  float a_g = 0.5f, ph_g = 2.0f;
  float a_b = 0.5f, ph_b = 4.0f;
  float freq = 1.0f;
  float rr = a_r * cosf(2.f * (float) M_PI * freq * v + ph_r) + (1.0f - a_r);
  float gg = a_g * cosf(2.f * (float) M_PI * freq * v + ph_g) + (1.0f - a_g);
  float bb = a_b * cosf(2.f * (float) M_PI * freq * v + ph_b) + (1.0f - a_b);
  *r = (uint8_t) (rr * 255.0f);
  *g = (uint8_t) (gg * 255.0f);
  *b = (uint8_t) (bb * 255.0f);
}

static void wave(float v, uint8_t *r, uint8_t *g, uint8_t *b)
{
  *r = (uint8_t) (v * 255.0f);
  *g = (uint8_t) (v * 10.0f);
  *b = (uint8_t) (v * 255.0f);
}

static void rgba_fill_from_noise(uint8_t *rgba, int W, int H, int n, int mode,
                                 float dz, fnl_state *noise)
{
#pragma omp parallel for
  for (int y = 0; y < H; ++y)
  {
    for (int x = 0; x < W; ++x)
    {
      float v = (fnlGetNoise3D(noise, (float) x, (float) y, (float) n * dz) + 1.0f) * 0.5f;
      if (v < 0)
        v = 0;
      if (v > 1)
        v = 1;
      size_t i = (size_t) (y * W + x) * 4;
      uint8_t r, g, b;
      switch (mode)
      {
      case 1:
        wave(v, &r, &g, &b);
        break;
      case 2:
        rainbow(v, &r, &g, &b);
        break;
      default:
        dual(v, &r, &g, &b);
        break;
      }
      rgba[i + 0] = r;
      rgba[i + 1] = g;
      rgba[i + 2] = b;
      rgba[i + 3] = 255;   // opaque alpha
    }
  }
}

// -------- GIF path --------
static int write_gif(const char *filename, int W, int H, int fps, int frames,
                     int bpp, fnl_state *noise, int mode, float dz)
{
  MsfGifState gif = {0};
  if (!msf_gif_begin(&gif, W, H))
  {
    fprintf(stderr, "Failed to begin GIF\n");
    return -1;
  }
  uint8_t *rgba = (uint8_t *) malloc((size_t) W * H * 4);
  if (!rgba)
  {
    fprintf(stderr, "Allocation failure\n");
    return -1;
  }

  for (int n = 0; n < frames; ++n)
  {
    rgba_fill_from_noise(rgba, W, H, n, mode, dz, noise);
    msf_gif_frame(&gif, rgba, (int) (100 / fps), bpp, W * 4);
  }

  MsfGifResult result = msf_gif_end(&gif);
  if (result.data)
  {
    FILE *f = fopen(filename, "wb");
    if (!f)
    {
      fprintf(stderr, "Failed to open %s\n", filename);
      free(rgba);
      msf_gif_free(result);
      return -1;
    }
    fwrite(result.data, 1, result.dataSize, f);
    fclose(f);
    msf_gif_free(result);
  }
  free(rgba);
  return 0;
}

// -------- WebM (VP9) path using FFmpeg --------
static int write_webm_vp9(const char *filename, int W, int H, int fps, int frames,
                          fnl_state *noise, int mode, float dz)
{
  int ret = 0;

  av_log_set_level(AV_LOG_ERROR);

  AVFormatContext *oc = NULL;
  AVStream *st = NULL;
  AVCodecContext *cc = NULL;
  const AVCodec *codec = NULL;
  struct SwsContext *sws = NULL;
  AVFrame *frame = NULL;
  AVFrame *rgb = NULL;
  AVPacket *pkt = NULL;

  // Container
  if ((ret = avformat_alloc_output_context2(&oc, NULL, "webm", filename)) < 0 || !oc)
  {
    fprintf(stderr, "avformat_alloc_output_context2 failed: %d\n", ret);
    return -1;
  }

  // Codec (VP9 via libvpx-vp9 if present, otherwise native "vp9")
  codec = avcodec_find_encoder_by_name("libvpx-vp9");
  if (!codec)
    codec = avcodec_find_encoder(AV_CODEC_ID_VP9);
  if (!codec)
  {
    fprintf(stderr, "No VP9 encoder found in this FFmpeg build\n");
    ret = -1;
    goto cleanup;
  }

  st = avformat_new_stream(oc, NULL);
  if (!st)
  {
    fprintf(stderr, "avformat_new_stream failed\n");
    ret = -1;
    goto cleanup;
  }

  cc = avcodec_alloc_context3(codec);
  if (!cc)
  {
    fprintf(stderr, "avcodec_alloc_context3 failed\n");
    ret = -1;
    goto cleanup;
  }

  cc->codec_id = codec->id;
  cc->width = W;
  cc->height = H;
  cc->time_base = (AVRational) {1, fps};
  cc->framerate = (AVRational) {fps, 1};
  cc->gop_size = fps * 2;
  cc->max_b_frames = 0;
  cc->pix_fmt = AV_PIX_FMT_YUV420P;
  // Quality-based rate control
  if (codec->id == AV_CODEC_ID_VP9)
  {
    av_opt_set(cc->priv_data, "crf", "32", 0);
    av_opt_set(cc->priv_data, "b", "0", 0);
    av_opt_set(cc->priv_data, "row-mt", "1", 0);
  }

  if (oc->oformat->flags & AVFMT_GLOBALHEADER)
    cc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  ret = avcodec_open2(cc, codec, NULL);
  if (ret < 0)
  {
    fprintf(stderr, "avcodec_open2 failed: %d\n", ret);
    goto cleanup;
  }

  ret = avcodec_parameters_from_context(st->codecpar, cc);
  if (ret < 0)
  {
    fprintf(stderr, "avcodec_parameters_from_context failed\n");
    goto cleanup;
  }

  // Open IO
  if (!(oc->oformat->flags & AVFMT_NOFILE))
  {
    ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
    if (ret < 0)
    {
      fprintf(stderr, "avio_open failed\n");
      goto cleanup;
    }
  }

  ret = avformat_write_header(oc, NULL);
  if (ret < 0)
  {
    fprintf(stderr, "avformat_write_header failed\n");
    goto cleanup;
  }

  // Frames and conversion
  rgb = av_frame_alloc();
  frame = av_frame_alloc();
  pkt = av_packet_alloc();
  if (!rgb || !frame || !pkt)
  {
    fprintf(stderr, "frame/pkt alloc failed\n");
    ret = -1;
    goto cleanup;
  }

  rgb->format = AV_PIX_FMT_RGBA;
  rgb->width = W;
  rgb->height = H;
  ret = av_frame_get_buffer(rgb, 32);
  if (ret < 0)
  {
    fprintf(stderr, "av_frame_get_buffer rgb failed\n");
    goto cleanup;
  }

  frame->format = cc->pix_fmt;
  frame->width = W;
  frame->height = H;
  ret = av_frame_get_buffer(frame, 32);
  if (ret < 0)
  {
    fprintf(stderr, "av_frame_get_buffer yuv failed\n");
    goto cleanup;
  }

  sws = sws_getContext(W, H, AV_PIX_FMT_RGBA, W, H, cc->pix_fmt,
                       SWS_BILINEAR, NULL, NULL, NULL);
  if (!sws)
  {
    fprintf(stderr, "sws_getContext failed\n");
    ret = -1;
    goto cleanup;
  }

  // Generate and encode
  for (int n = 0; n < frames; ++n)
  {
    // Fill rgb from noise
    rgba_fill_from_noise(rgb->data[0], W, H, n, mode, dz, noise);

    // Convert to YUV420P
    sws_scale(sws, (const uint8_t *const *) rgb->data, rgb->linesize, 0, H,
              frame->data, frame->linesize);

    frame->pts = n;

    ret = avcodec_send_frame(cc, frame);
    if (ret < 0)
    {
      fprintf(stderr, "send_frame failed: %d\n", ret);
      goto cleanup;
    }

    // Drain packets
    while ((ret = avcodec_receive_packet(cc, pkt)) == 0)
    {
      pkt->stream_index = st->index;
      av_packet_rescale_ts(pkt, cc->time_base, st->time_base);
      ret = av_interleaved_write_frame(oc, pkt);
      av_packet_unref(pkt);
      if (ret < 0)
      {
        fprintf(stderr, "write_frame failed: %d\n", ret);
        goto cleanup;
      }
    }
    if (ret == AVERROR(EAGAIN))
      ret = 0;
    if (ret < 0)
      goto cleanup;
  }

  // Flush
  ret = avcodec_send_frame(cc, NULL);
  if (ret >= 0)
  {
    while ((ret = avcodec_receive_packet(cc, pkt)) == 0)
    {
      pkt->stream_index = st->index;
      av_packet_rescale_ts(pkt, cc->time_base, st->time_base);
      if ((ret = av_interleaved_write_frame(oc, pkt)) < 0)
      {
        fprintf(stderr, "write_frame (flush) failed: %d\n", ret);
        goto cleanup;
      }
      av_packet_unref(pkt);
    }
    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
      ret = 0;
  }

  ret = av_write_trailer(oc);
  if (ret < 0)
  {
    fprintf(stderr, "av_write_trailer failed\n");
    goto cleanup;
  }

cleanup:
  if (sws)
    sws_freeContext(sws);
  if (pkt)
    av_packet_free(&pkt);
  if (rgb)
    av_frame_free(&rgb);
  if (frame)
    av_frame_free(&frame);
  if (cc)
    avcodec_free_context(&cc);
  if (oc)
  {
    if (!(oc->oformat->flags & AVFMT_NOFILE) && oc->pb)
      avio_closep(&oc->pb);
    avformat_free_context(oc);
  }
  return ret;
}

static int sanitize_option(void *opt, int min, int max)
{
  int *i = (int *) opt;
  if (*i < min)
  {
    *i = min;
    return 1;
  }
  else if (*i > max)
  {
    *i = max;
    return 1;
  }
  return 0;
}

static int sanitize_option_f(void *opt, float min, float max)
{
  float *f = (float *) opt;
  if (*f < min)
  {
    *f = min;
    return 1;
  }
  else if (*f > max)
  {
    *f = max;
    return 1;
  }
  return 0;
}

int main(int argc, char **argv)
{
  int opt;
  int screenW = 720, screenH = 480;
  int mode = 0;
  float dz = 0.3f, freq = 0.01f;
  int length = 10, fps = 30, bpp = 16;
  OutFmt outFmt = OUT_GIF;
  char filename[512] = {0};

  int sanitize_flag = 0;

  while ((opt = getopt(argc, argv, "w:h:m:d:l:i:b:f:F:O:")) != -1)
  {
    switch (opt)
    {
    case 'w':
      screenW = atoi(optarg);
      sanitize_flag = sanitize_option(&screenW, 1, 2560);
      break;
    case 'h':
      screenH = atoi(optarg);
      sanitize_flag = sanitize_option(&screenH, 1, 1440);
      break;
    case 'm':
      mode = atoi(optarg);
      sanitize_flag = sanitize_option(&mode, 0, 2);
      break;
    case 'd':
      dz = (float) atof(optarg);
      sanitize_flag = sanitize_option_f(&dz, 0.0f, 1.0f);
      break;
    case 'l':
      length = atoi(optarg);
      sanitize_flag = sanitize_option(&length, 1, 100);
      break;
    case 'i':
      fps = atoi(optarg);
      sanitize_flag = sanitize_option(&fps, 1, 100);
      break;
    case 'b':
      bpp = atoi(optarg);
      sanitize_flag = sanitize_option(&bpp, 1, 16);
      break;
    case 'f':
      freq = (float) atof(optarg);
      sanitize_flag = sanitize_option_f(&freq, 0.0f, 1.0f);
      break;
    case 'F':
      strncpy(filename, optarg, sizeof(filename) - 1);
      filename[sizeof(filename) - 1] = '\0';
      break;
    case 'O':
      if (strncmp(optarg, "gif", 3) == 0)
        outFmt = OUT_GIF;
      else if (strncmp(optarg, "webm", 4) == 0)
        outFmt = OUT_WEBM;
      else
      {
        fprintf(stderr, "Unknown format '%s' (gif|webm)\n", optarg);
        return 1;
      }
      break;
    default:
      fprintf(stderr, "Usage:\n"
                      "\t-w W\twidth\n"
                      "\t-h H\theight\n"
                      "\t-m M\tmode (0: dual, 1: wave, 2: rainbow)\n"
                      "\t-d D\tstep in noise Z axis\n"
                      "\t-l L\tlength seconds\n"
                      "\t-i I\tfps\n"
                      "\t-b B\tbytes per pixel (GIF path)\n"
                      "\t-f F\tnoise frequency\n"
                      "\t-O fmt\toutput format: gif|webm (default gif)\n"
                      "\t-F name\toutput filename\n");
      return 1;
    }
  }

  if (sanitize_flag)
  {
    fprintf(stderr, "Invalid options\n");
    return 1;
  }

  int frames = fps * length;

  // FastNoiseLite setup
  fnl_state noise = fnlCreateState();
  noise.noise_type = FNL_NOISE_OPENSIMPLEX2;
  noise.frequency = freq;
  noise.fractal_type = FNL_FRACTAL_RIDGED;
  noise.octaves = 2;
  noise.lacunarity = 0.95f;

  if (filename[0] == '\0')
  {
    snprintf(filename, sizeof(filename), "%s", outFmt == OUT_GIF ? "noise.gif" : "noise.webm");
  }

  if (outFmt == OUT_GIF)
  {
    return write_gif(filename, screenW, screenH, fps, frames, bpp, &noise, mode, dz) == 0 ? 0 : 1;
  }
  else
  {
    return write_webm_vp9(filename, screenW, screenH, fps, frames, &noise, mode, dz) == 0 ? 0 : 1;
  }
}
