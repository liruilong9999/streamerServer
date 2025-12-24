# vedio_server



live555下载地址

http://download.videolan.org/pub/contrib/live555/

客户端请求视频，根据视频名字获得视频文件

项目使用版本

live.2017.09.12.tar.gz

opencv4.10.0

ffmpeg 4.3.1

SDL2.30.12



## ffmpeg笔记

### 1. `AVFormatContext * pFormatCtx`

- **类型**：`AVFormatContext` 是 FFmpeg 中一个非常重要的结构体，它用于描述多媒体流的格式。可以理解为媒体文件或设备的上下文，它保存了与流（视频、音频等）相关的元数据和文件信息。
- **作用**：
  - 用于存储媒体文件的基本信息，如流的数量、格式、编码器、解码器等。
  - 在打开文件或设备时，FFmpeg 会为每个输入源（文件或设备）分配一个 `AVFormatContext`，并通过这个上下文来执行流的读写操作。
- **常见操作**：
  - `avformat_open_input()`：打开媒体文件或设备。
  - `avformat_find_stream_info()`：查找并解析媒体流信息。
  - `avformat_close_input()`：关闭输入流并释放资源。

### 2. `AVInputFormat * ifmt`

- **类型**：`AVInputFormat` 是一个指向 `AVInputFormat` 类型的指针，它用于指定输入格式。
- **作用**：
  - `AVInputFormat` 描述了如何解码输入数据。对于文件来说，它表示文件格式（例如 `.mp4`、`.avi`），而对于设备来说，它指定了使用哪个设备（如 `vfwcap` 或 `dshow`）。
  - 在本例中，它用于选择输入设备的类型，例如 `vfwcap`（用于 Windows 系统的视频捕获设备）或 `dshow`（DirectShow）。
- **常见操作**：
  - `av_find_input_format()`：查找支持的输入格式。
  - 在打开输入流时，`ifmt` 会传递给 `avformat_open_input()` 来告知 FFmpeg 使用何种输入格式。

### 3. `int i, videoindex`

- **类型**：`int` 用于循环遍历和存储索引。
- **作用**：
  - `i`：在遍历视频流时用于循环遍历所有流，检查视频流的位置。
  - `videoindex`：存储视频流的索引值。因为一个媒体文件或设备可能包含多个流（例如视频流、音频流），因此 `videoindex` 用于保存视频流的索引位置。
- **常见操作**：
  - 遍历流时，查找 `AVMEDIA_TYPE_VIDEO` 类型的流，将 `videoindex` 设置为视频流的索引。

### 4. `int numBytes`

- **类型**：`int` 用于存储字节数。
- **作用**：
  - 计算图像缓冲区的大小，`numBytes` 通常用于存储 RGB 图像缓冲区的大小。
  - 在转换图像格式时（例如从 YUV 到 RGB），会用 `numBytes` 来分配足够的内存空间来存储转换后的图像。
- **常见操作**：
  - `avpicture_get_size()`：计算图像的大小（例如，RGB 图像的大小）。
  - `av_malloc()`：根据 `numBytes` 分配内存。

### 5. `int ret, got_picture`

- **类型**：`int` 是用来存储返回值和状态标识。
- **作用**：
  - `ret`：用于存储解码操作的返回值。例如，`avcodec_decode_video2()` 会返回一个整数，表示解码成功或失败。
  - `got_picture`：是一个标识变量，指示是否成功解码了一帧图像。它通常在解码函数中作为参数传递给 `avcodec_decode_video2()`，如果解码成功，`got_picture` 会被设置为非零值。
- **常见操作**：
  - `avcodec_decode_video2()`：解码视频帧，`got_picture` 用于指示是否成功解码。

### 6. `AVCodecContext * pCodecCtx`

- **类型**：`AVCodecContext` 是 FFmpeg 中用于描述编解码器的上下文结构体。
- **作用**：
  - 这个上下文包含了解码器或编码器的配置信息，包括编解码器类型、视频帧的宽高、像素格式、时间基等。
  - 每个视频流（或音频流）都会有一个对应的 `AVCodecContext`，通过它来管理解码和编码的状态。
- **常见操作**：
  - `avcodec_find_decoder()`：查找解码器。
  - `avcodec_open2()`：打开解码器。
  - `avcodec_close()`：关闭解码器。

### 7. `AVCodec * pCodec`

- **类型**：`AVCodec` 是一个指针，指向 `AVCodec` 结构体，表示解码器或编码器的类型。
- **作用**：
  - `pCodec` 用来指示用于解码的编解码器，例如，H.264、H.265 等。
  - `AVCodec` 结构体包含了解码器的名称、ID、操作方法等信息。
- **常见操作**：
  - `avcodec_find_decoder()`：通过视频流的编码格式查找对应的解码器。
  - `avcodec_find_encoder()`：查找编码器。

### 8. `AVFrame * pFrame, *pFrameRGB`

- **类型**：`AVFrame` 是一个结构体，用于表示一个解码后的帧，包含原始视频或音频数据。
- **作用**：
  - `pFrame`：用于存储解码后的原始视频帧（如 YUV 格式）。
  - `pFrameRGB`：用于存储转换后的 RGB 图像帧。因为 Qt 显示需要 RGB 格式，所以需要将视频帧从 YUV 格式转换为 RGB 格式。
- **常见操作**：
  - `av_frame_alloc()`：分配一个新的 `AVFrame`。
  - `avpicture_fill()`：填充 RGB 图像数据。

### 9. `AVPacket * packet`

- **类型**：`AVPacket` 是一个数据包结构，存储了音视频数据流的压缩数据（例如，H.264 数据流）。
- **作用**：
  - `packet` 存储从媒体源（文件、设备、网络流等）读取的数据包。每个包可能包含多个视频帧或音频帧的数据。
  - `av_read_frame()` 用来从输入流中读取数据包，通常是压缩的音视频数据，之后需要通过解码器进行解码。
- **常见操作**：
  - `av_read_frame()`：读取输入流中的数据包。
  - `avcodec_decode_video2()`：解码数据包中的视频帧。

### 10. `uint8_t * out_buffer`

- **类型**：`uint8_t *` 是一个指向 `uint8_t`（无符号字节）的指针，用于存储原始的字节数据。
- **作用**：
  - `out_buffer` 存储解码后的 RGB 数据。在图像格式转换（如 YUV 到 RGB）时，需要一个缓冲区来保存转换后的数据。
  - 在使用 `sws_scale()` 进行像素格式转换时，转换后的 RGB 数据会存储在该缓冲区中。
- **常见操作**：
  - `av_malloc()`：为 `out_buffer` 分配内存。
  - `sws_scale()`：使用该缓冲区存储转换后的图像数据。

