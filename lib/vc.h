#define VC_DEBUG

typedef struct {
	unsigned char *data;
	int width, height;
	int channels;
	int levels;
	int bytesperline;
} IVC;

typedef struct {
	int x, y, width, height;
	int area;
	int xc, yc;
	int perimeter;
	int label;
} OVC;

IVC *vc_image_new(int width, int height, int channels, int levels);
IVC *vc_image_free(IVC *image);
IVC *vc_read_image(char *filename);
int vc_write_image(char *filename, IVC *image);
int vc_rgb_negative(IVC *srcdst);
int vc_gray_negative(IVC *srcdst);
int vc_rgb_get_red_gray(IVC *srcdst);
int vc_rgb_get_blue_gray(IVC *srcdst);
int vc_rgb_get_green_gray(IVC *srcdst);
int vc_rgb_to_gray(IVC *src, IVC *dst);
int vc_rgb_to_hsv(IVC *src, IVC *dst);
int vc_hsv_segmentation(IVC *src, IVC *dst, int hmin, int hmax, int smin, int smax, int vmin, int vmax);
int vc_scale_gray_to_rgb(IVC *src, IVC *dst);
int count_pixels_bw(IVC *image, int *white, int *black);
int count_colors_mask(IVC *image, IVC *mask, int *red, int *yellow, int *green, int *blue, int *brain);
int vc_gray_to_binary(IVC *src, IVC *dst, int threshold);
int vc_gray_to_binary_global_mean(IVC *src, IVC *dst);
int vc_gray_to_binary_midpoint(IVC *src, IVC *dst, int kernel);
int vc_gray_to_binary_bernsen(IVC *src, IVC *dst, int kernel);
int vc_binary_dilate(IVC *src, IVC *dst, int kernel);
int vc_binary_erode(IVC *src, IVC *dst, int kernel);
int vc_binary_open(IVC *src, IVC *dst, int kernel, int kernel2);
int vc_binary_close(IVC *src, IVC *dst, int kernel, int kernel2);
int vc_erode_minus_dilate(IVC *img1, IVC *img2, IVC *dst);
int vc_gray_mask(IVC *src, IVC *mask, IVC *dst);
OVC* vc_binary_blob_labelling(IVC *src, IVC *dst, int *nlabels);
int vc_binary_blob_info(IVC *src, OVC *blobs, int nblobs);
int vc_histogram(IVC *src, int *histogram);
int vc_histogram_image(IVC *src, IVC *dst);
int vc_histogram_equalization(IVC *src, IVC *dst);
int vc_gray_edge_prewitt(IVC *src, IVC *dst, float th);
int vc_gray_edge_sobel(IVC *src, IVC *dst, float th);
int vc_gray_lowpass_mean_filter(IVC *src, IVC *dst, int kernelsize);
int vc_gray_lowpass_median_filter(IVC *src, IVC *dst, int kernelsize);
int vc_gray_lowpass_gaussian_filter(IVC *src, IVC *dst);
