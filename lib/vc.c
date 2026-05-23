
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <malloc.h>
#include "vc.h"
#include "math.h"
#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif


// Aloca memoria para uma imagem IVC
IVC *vc_image_new(int width, int height, int channels, int levels)
{
	IVC *image = (IVC *) malloc(sizeof(IVC));

	if(image == NULL) return NULL;
	if((levels <= 0) || (levels > 255)) return NULL;

	image->width = width;
	image->height = height;
	image->channels = channels;
	image->levels = levels;
	image->bytesperline = image->width * image->channels;
	image->data = (unsigned char *) malloc(image->width * image->height * image->channels * sizeof(char));

	if(image->data == NULL)
	{
		return vc_image_free(image);
	}

	return image;
}


// Liberta memoria de uma imagem IVC
IVC *vc_image_free(IVC *image)
{
	if(image != NULL)
	{
		if(image->data != NULL)
		{
			free(image->data);
			image->data = NULL;
		}

		free(image);
		image = NULL;
	}

	return image;
}

char *netpbm_get_token(FILE *file, char *tok, int len)
{
	char *t;
	int c;
	
	for(;;)
	{
		while(isspace(c = getc(file)));
		if(c != '#') break;
		do c = getc(file);
		while((c != '\n') && (c != EOF));
		if(c == EOF) break;
	}
	
	t = tok;
	
	if(c != EOF)
	{
		do
		{
			*t++ = c;
			c = getc(file);
		} while((!isspace(c)) && (c != '#') && (c != EOF) && (t - tok < len - 1));
		
		if(c == '#') ungetc(c, file);
	}
	
	*t = 0;
	
	return tok;
}

long int unsigned_char_to_bit(unsigned char *datauchar, unsigned char *databit, int width, int height)
{
	int x, y;
	int countbits;
	long int pos, counttotalbytes;
	unsigned char *p = databit;

	*p = 0;
	countbits = 1;
	counttotalbytes = 0;

	for(y=0; y<height; y++)
	{
		for(x=0; x<width; x++)
		{
			pos = width * y + x;

			if(countbits <= 8)
			{
				
				*p |= (datauchar[pos] == 0) << (8 - countbits);

				countbits++;
			}
			if((countbits > 8) || (x == width - 1))
			{
				p++;
				*p = 0;
				countbits = 1;
				counttotalbytes++;
			}
		}
	}

	return counttotalbytes;
}

void bit_to_unsigned_char(unsigned char *databit, unsigned char *datauchar, int width, int height)
{
	int x, y;
	int countbits;
	long int pos;
	unsigned char *p = databit;

	countbits = 1;

	for(y=0; y<height; y++)
	{
		for(x=0; x<width; x++)
		{
			pos = width * y + x;

			if(countbits <= 8)
			{

				datauchar[pos] = (*p & (1 << (8 - countbits))) ? 0 : 1;
				
				countbits++;
			}
			if((countbits > 8) || (x == width - 1))
			{
				p++;
				countbits = 1;
			}
		}
	}
}

IVC *vc_read_image(char *filename)
{
	FILE *file = NULL;
	IVC *image = NULL;
	unsigned char *tmp;
	char tok[20];
	long int size, sizeofbinarydata;
	int width, height, channels;
	int levels = 255;
	int v;
	
	if((file = fopen(filename, "rb")) != NULL)
	{
		netpbm_get_token(file, tok, sizeof(tok));

		if(strcmp(tok, "P4") == 0) { channels = 1; levels = 1; }
		else if(strcmp(tok, "P5") == 0) channels = 1;
		else if(strcmp(tok, "P6") == 0) channels = 3;
		else
		{
			#ifdef VC_DEBUG
			printf("ERROR -> vc_read_image():\n\tFile is not a valid PBM, PGM or PPM file.\n\tBad magic number!\n");
			#endif

			fclose(file);
			return NULL;
		}
		
		if(levels == 1)
		{
			if(sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d", &width) != 1 || 
			   sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d", &height) != 1)
			{
				#ifdef VC_DEBUG
				printf("ERROR -> vc_read_image():\n\tFile is not a valid PBM file.\n\tBad size!\n");
				#endif

				fclose(file);
				return NULL;
			}

			image = vc_image_new(width, height, channels, levels);
			if(image == NULL) return NULL;

			sizeofbinarydata = (image->width / 8 + ((image->width % 8) ? 1 : 0)) * image->height;
			tmp = (unsigned char *) malloc(sizeofbinarydata);
			if(tmp == NULL) return 0;

			#ifdef VC_DEBUG
			printf("\nchannels=%d w=%d h=%d levels=%d\n", image->channels, image->width, image->height, levels);
			#endif

			if((v = fread(tmp, sizeof(unsigned char), sizeofbinarydata, file)) != sizeofbinarydata)
			{
				#ifdef VC_DEBUG
				printf("ERROR -> vc_read_image():\n\tPremature EOF on file.\n");
				#endif

				vc_image_free(image);
				fclose(file);
				free(tmp);
				return NULL;
			}

			bit_to_unsigned_char(tmp, image->data, image->width, image->height);

			free(tmp);
		}
		else
		{
			if(sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d", &width) != 1 || 
			   sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d", &height) != 1 || 
			   sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d", &levels) != 1 || levels <= 0 || levels > 255)
			{
				#ifdef VC_DEBUG
				printf("ERROR -> vc_read_image():\n\tFile is not a valid PGM or PPM file.\n\tBad size!\n");
				#endif

				fclose(file);
				return NULL;
			}

			image = vc_image_new(width, height, channels, levels);
			if(image == NULL) return NULL;

			#ifdef VC_DEBUG
			printf("\nchannels=%d w=%d h=%d levels=%d\n", image->channels, image->width, image->height, levels);
			#endif

			size = image->width * image->height * image->channels;

			if((v = fread(image->data, sizeof(unsigned char), size, file)) != size)
			{
				#ifdef VC_DEBUG
				printf("ERROR -> vc_read_image():\n\tPremature EOF on file.\n");
				#endif

				vc_image_free(image);
				fclose(file);
				return NULL;
			}
		}
		
		fclose(file);
	}
	else
	{
		#ifdef VC_DEBUG
		printf("ERROR -> vc_read_image():\n\tFile not found.\n");
		#endif
	}
	
	return image;
}

int vc_write_image(char *filename, IVC *image)
{
	FILE *file = NULL;
	unsigned char *tmp;
	long int totalbytes, sizeofbinarydata;
	
	if(image == NULL) return 0;

	if((file = fopen(filename, "wb")) != NULL)
	{
		if(image->levels == 1)
		{
			sizeofbinarydata = (image->width / 8 + ((image->width % 8) ? 1 : 0)) * image->height + 1;
			tmp = (unsigned char *) malloc(sizeofbinarydata);
			if(tmp == NULL) return 0;
			
			fprintf(file, "%s %d %d\n", "P4", image->width, image->height);
			
			totalbytes = unsigned_char_to_bit(image->data, tmp, image->width, image->height);
			printf("Total = %ld\n", totalbytes);
			if(fwrite(tmp, sizeof(unsigned char), totalbytes, file) != totalbytes)
			{
				#ifdef VC_DEBUG
				fprintf(stderr, "ERROR -> vc_read_image():\n\tError writing PBM, PGM or PPM file.\n");
				#endif

				fclose(file);
				free(tmp);
				return 0;
			}

			free(tmp);
		}
		else
		{
			fprintf(file, "%s %d %d 255\n", (image->channels == 1) ? "P5" : "P6", image->width, image->height);
		
			if(fwrite(image->data, image->bytesperline, image->height, file) != image->height)
			{
				#ifdef VC_DEBUG
				fprintf(stderr, "ERROR -> vc_read_image():\n\tError writing PBM, PGM or PPM file.\n");
				#endif

				fclose(file);
				return 0;
			}
		}
		
		fclose(file);

		return 1;
	}
	
	return 0;
}

int vc_gray_negative(IVC *srcdst)
{
    unsigned char *data = (unsigned char *) srcdst->data;
    int width = srcdst->width;
    int height = srcdst->height;
    int bytesperline = srcdst->width * srcdst->channels;
    int channels = srcdst->channels;
    int x, y;
    long int pos;

    if((srcdst->width <= 0) || (srcdst->height <= 0) || (srcdst->data == NULL)) return 0;
    if(channels != 1) return 0;

    for(y=0; y<height; y++)
    {
        for(x=0; x<width; x++)
        {
            pos = y * bytesperline + x * channels;

            data[pos] = 255 - data[pos];
        }
    }

    return 1;
}

int vc_rgb_negative(IVC *srcdst)
{
    unsigned char *data = (unsigned char *) srcdst->data;
    int width = srcdst->width;
    int height = srcdst->height;
    int bytesperline = srcdst->width * srcdst->channels;
    int channels = srcdst->channels;
    int x, y;
    long int pos;

    if((srcdst->width <= 0) || (srcdst->height <= 0) || (srcdst->data == NULL)) return 0;
    if(channels != 3) return 0;

    for(y=0; y<height; y++)
    {
        for(x=0; x<width; x++)
        {
            pos = y * bytesperline + x * channels;

            data[pos] = 255 - data[pos];
            data[pos + 1] = 255 - data[pos + 1];
            data[pos + 2] = 255 - data[pos + 2];
        }
    }

    return 1;
}

int vc_rgb_get_red_gray(IVC *srcdst)
{
    unsigned char *data = (unsigned char *) srcdst->data;
    int width = srcdst->width;
    int height = srcdst->height;
    int bytesperline = srcdst->width * srcdst->channels;
    int channels = srcdst->channels;
    int x, y;
    long int pos;

    if((srcdst->width <= 0) || (srcdst->height <= 0) || (srcdst->data == NULL)) return 0;
    if(channels != 3) return 0;

    for(y=0; y<height; y++)
    {
        for(x=0; x<width; x++)
        {
            pos = y * bytesperline + x * channels;

            data[pos + 1] = data[pos];
            data[pos + 2] = data[pos];
        }
    }

    return 1;
}

int vc_rgb_get_blue_gray(IVC *srcdst)
{
    unsigned char *data = (unsigned char *) srcdst->data;
    int width = srcdst->width;
    int height = srcdst->height;
    int bytesperline = srcdst->width * srcdst->channels;
    int channels = srcdst->channels;
    int x, y;
    long int pos;

    if((srcdst->width <= 0) || (srcdst->height <= 0) || (srcdst->data == NULL)) return 0;
    if(channels != 3) return 0;

    for(y=0; y<height; y++)
    {
        for(x=0; x<width; x++)
        {
            pos = y * bytesperline + x * channels;

            data[pos] = data[pos + 2]; 
            data[pos + 1] = data[pos + 2]; 
        }
    }

    return 1;
}

int vc_rgb_get_green_gray(IVC *srcdst)
{
    unsigned char *data = (unsigned char *) srcdst->data;
    int width = srcdst->width;
    int height = srcdst->height;
    int bytesperline = srcdst->width * srcdst->channels;
    int channels = srcdst->channels;
    int x, y;
    long int pos;

    if((srcdst->width <= 0) || (srcdst->height <= 0) || (srcdst->data == NULL)) return 0;
    if(channels != 3) return 0;

    for(y=0; y<height; y++)
    {
        for(x=0; x<width; x++)
        {
            pos = y * bytesperline + x * channels;

            data[pos] = data[pos + 1]; 
            data[pos + 2] = data[pos + 1]; 
        }
    }

    return 1;
}


// Converte RGB para imagem cinzenta
int vc_rgb_to_gray(IVC *src, IVC *dst)
{
    unsigned char *datasrc = (unsigned char *) src->data;
    int bytesperline_src = src->width * src->channels;
    int channels_src = src->channels;
    unsigned char *datadst = (unsigned char *) dst->data;
    int bytesperline_dst = dst->width * dst->channels;
    int channels_dst = dst->channels;
    int width = src->width;
    int height = src->height;
    int x, y;
    long int pos_src, pos_dst;
    float rf, gf, bf;

    if((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
    if((src->width != dst->width) || (src->height != dst->height)) return 0;
    if((src->channels != 3) || (dst->channels != 1)) return 0;

    for(y=0; y<height; y++)
    {
        for(x=0; x<width; x++)
        {
            pos_src = y * bytesperline_src + x * channels_src;
            pos_dst = y * bytesperline_dst + x * channels_dst;

            rf = (float) datasrc[pos_src];
            gf = (float) datasrc[pos_src + 1];
            bf = (float) datasrc[pos_src + 2];

            datadst[pos_dst] = (unsigned char) ((rf * 0.299) + (gf * 0.587) + (bf * 0.114));
        }
    }

    return 1;
}


// Converte RGB para HSV
int vc_rgb_to_hsv(IVC *src, IVC *dst){

    unsigned char *datasrc = (unsigned char *) src->data;
    int bytesperline_src = src->width * src->channels;
    int channels_src = src->channels;
    unsigned char *datadst = (unsigned char *) dst->data;
    int bytesperline_dst = dst->width * dst->channels;
    int channels_dst = dst->channels;
    int width = src->width;
    int height = src->height;
    int x;
    long int pos_src, pos_dst;
    float  hf, sf, vf;

	for(x = 0; x < width * height * channels_src; x= x+channels_src)
	{
		int r = datasrc[x];
		int g = datasrc[x + 1];
		int b = datasrc[x + 2];

		int max = r > g ? (r > b ? r : b) : (g > b ? g : b);
		int min = r < g ? (r < b ? r : b) : (g < b ? g : b);

		vf = max;

		if(max != 0)
			sf = 255 * (max - min) / max;
		else
			sf = 0;

		if(sf != 0)
		{
			if(max == r  && g >= b)
				hf = 60.0 * (g - b) / (max - min);
			else if(max == r  && g < b)
				hf = 360.0 + 60.0 * (g - b) / (max - min);
			else if(max == g)
				hf = 120.0 + 60.0 * (b - r) / (max - min);
			else
				hf = 240.0 + 60.0 * (r - g) / (max - min);
		}
		else
			hf = 0;

		datadst[x] = hf / 360.0 * 255.0;
		datadst[x + 1] = sf ;
		datadst[x + 2] = vf;
	}

	return 1;

	}


// Segmenta objetos por intervalo HSV
	int vc_hsv_segmentation(IVC *src, IVC *dst, int hmin, int hmax, int smin, int smax, int vmin, int vmax){
	unsigned char *datasrc = (unsigned char *)src->data;
	unsigned char *datadst = (unsigned char *)dst->data;
	int width = src->width;
	int height = src->height;
	int bytesperline = src->bytesperline;
	int channels = src->channels;
	int i, size;
	int hmin_deg, hmax_deg, hmin_255, hmax_255;
	int hue_ok, sv_ok;

	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if ((src->width != dst->width) || (src->height != dst->height) || (src->channels != dst->channels)) return 0;
	if (channels != 3) return 0;

	hmin_deg = hmin;
	hmax_deg = hmax;
	if (hmin_deg < 0) hmin_deg = 0;
	if (hmin_deg > 360) hmin_deg = 360;
	if (hmax_deg < 0) hmax_deg = 0;
	if (hmax_deg > 360) hmax_deg = 360;
	hmin_255 = (hmin_deg * 255) / 360;
	hmax_255 = (hmax_deg * 255) / 360;

	size = width * height * channels;

	for (i = 0; i<size; i = i + channels)
	{
		if (hmin_255 <= hmax_255)
		{
			hue_ok = (datasrc[i] >= hmin_255) && (datasrc[i] <= hmax_255);
		}
		else
		{
			hue_ok = (datasrc[i] >= hmin_255) || (datasrc[i] <= hmax_255);
		}

		sv_ok = (datasrc[i + 1] >= smin) && (datasrc[i + 1] <= smax) &&
				(datasrc[i + 2] >= vmin) && (datasrc[i + 2] <= vmax);

		if (hue_ok && sv_ok)
		{
			datadst[i] = 255;
			datadst[i + 1] = 255;
			datadst[i + 2] = 255;
		}
		else
		{
			datadst[i] = 0;
			datadst[i + 1] = 0;
			datadst[i + 2] = 0;
		}
	}

	return 1;
}

int vc_scale_gray_to_rgb(IVC *src, IVC *dst) {
    unsigned char *datasrc = (unsigned char *) src->data;
    int bytesperline_src = src->width * src->channels;
    int channels_src = src->channels;
    unsigned char *datadst = (unsigned char *) dst->data;
    int bytesperline_dst = dst->width * dst->channels;
    int channels_dst = dst->channels;
    int width = src->width;
    int height = src->height;
    int x, y;
    long int pos_src, pos_dst;

    if((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
    if((src->width != dst->width) || (src->height != dst->height)) return 0;
    if((src->channels != 1) || (dst->channels != 3)) return 0;

    for(y=0; y<height; y++) {
        for(x=0; x<width; x++) {
            pos_src = y * bytesperline_src + x * channels_src;
            pos_dst = y * bytesperline_dst + x * channels_dst;

            int gray = datasrc[pos_src];
            int r, g, b;

            if (gray < 64) {
                r = 0;
                g = (gray * 255) / 64; 
                b = 255;
            } 
          
            else if (gray < 128) {
                r = 0;
                g = 255;
                b = 255 - ((gray - 64) * 255) / 64;
            } 
         
            else if (gray < 192) {
                r = ((gray - 128) * 255) / 64;
                g = 255;
                b = 0;
            } 
           
            else {
                r = 255;
                g = 255 - ((gray - 192) * 255) / 63; 
            }

            datadst[pos_dst]     = (unsigned char) r;
            datadst[pos_dst + 1] = (unsigned char) g;
            datadst[pos_dst + 2] = (unsigned char) b;
        }
    }

    return 1;
}

int count_pixels_bw(IVC *image, int *white, int *black)
{
    unsigned char *p = (unsigned char *)image->data;
    int width = image->width;
    int height = image->height;
    int channels = image->channels;

    *white = 0;
    *black = 0;

    for (int i = 0; i < width * height; i++)
    {
        int r = p[i * channels + 0];
        int g = p[i * channels + 1];
        int b = p[i * channels + 2];

        if (r == 255 && g == 255 && b == 255)
        {
            (*white)++;
        }
        else if (r == 0 && g == 0 && b == 0)
        {
            (*black)++;
        }
    }

    return 1;
}

int count_colors_mask(IVC *image, IVC *mask, int *red, int *yellow, int *green, int *blue, int *brain)
{
    unsigned char *pimg = (unsigned char *)image->data;
    unsigned char *pmask = (unsigned char *)mask->data;

    int width = image->width;
    int height = image->height;
    int channels = image->channels;

    *red = *yellow = *green = *blue = *brain = 0;

    for(int i = 0; i < width * height; i++)
    {
        int mr = pmask[i * channels + 0];

        if(mr == 0)
        {
            (*brain)++;

            int h = pimg[i * channels + 0];
            int s = pimg[i * channels + 1];
            int v = pimg[i * channels + 2];

            if(v < 50 || s < 50)
            {
                (*blue)++;
            }
            else
            {
                if((h >= 0 && h <= 32) || (h >= 206 && h <= 255))
                    (*red)++;

                else if(h >= 33 && h <= 50)
                    (*yellow)++;

                else if(h >= 51 && h <= 113)
                    (*green)++;

                else if(h >= 114 && h <= 205)
                    (*blue)++;
            }
        }
    }

    return 1;
}

int vc_gray_to_binary(IVC *src, IVC *dst, int threshold){

	unsigned char *datasrc = (unsigned char *) src->data;
	int bytesperline_src = src->width * src->channels;
	int channels_src = src->channels;
	unsigned char *datadst = (unsigned char *) dst->data;
	int bytesperline_dst = dst->width * dst->channels;
	int channels_dst = dst->channels;
	int width = src->width;
	int height = src->height;
	int x, y;
	long int pos_src, pos_dst;

	if((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if((src->width != dst->width) || (src->height != dst->height)) return 0;
	if((src->channels != 1) || (dst->channels != 1)) return 0;

	for(y=0; y<height; y++) {
		for(x=0; x<width; x++) {
			pos_src = y * bytesperline_src + x * channels_src;
			pos_dst = y * bytesperline_dst + x * channels_dst;

			datadst[pos_dst] = (datasrc[pos_src] >= threshold) ? 255 : 0;
		}
	}

	return 1;
}

int vc_gray_to_binary_global_mean(IVC *src, IVC *dst){

	unsigned char *datasrc = (unsigned char *) src->data;
	int bytesperline_src = src->width * src->channels;
	int channels_src = src->channels;
	unsigned char *datadst = (unsigned char *) dst->data;
	int bytesperline_dst = dst->width * dst->channels;
	int channels_dst = dst->channels;
	int width = src->width;
	int height = src->height;
	int x, y;
	long int pos_src, pos_dst;

	if((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if((src->width != dst->width) || (src->height != dst->height)) return 0;
	if((src->channels != 1) || (dst->channels != 1)) return 0;

	long int sum = 0;
	for(y=0; y<height; y++) {
		for(x=0; x<width; x++) {
			pos_src = y * bytesperline_src + x * channels_src;
			sum += datasrc[pos_src];
		}
	}

	int mean = sum / (width * height);

	return vc_gray_to_binary(src, dst, mean);
}

int vc_gray_to_binary_midpoint(IVC *src, IVC *dst, int kernel){
	unsigned char *datasrc = (unsigned char *) src->data;
	int bytesperline_src = src->width * src->channels;
	int channels_src = src->channels;
	unsigned char *datadst = (unsigned char *) dst->data;
	int bytesperline_dst = dst->width * dst->channels;
	int channels_dst = dst->channels;
	int width = src->width;
	int height = src->height;
	int x, y, kx, ky;
	long int pos_src, pos_dst;

	if((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if((src->width != dst->width) || (src->height != dst->height)) return 0;
	if((src->channels != 1) || (dst->channels != 1)) return 0;

	for(y=0; y<height; y++) {
		for(x=0; x<width; x++) {
			pos_src = y * bytesperline_src + x * channels_src;
			pos_dst = y * bytesperline_dst + x * channels_dst;

			int min = 255;
			int max = 0;

			for(ky=-kernel/2; ky<=kernel/2; ky++) {
				for(kx=-kernel/2; kx<=kernel/2; kx++) {
					int nx = x + kx;
					int ny = y + ky;

					if(nx >= 0 && nx < width && ny >= 0 && ny < height) {
						long int npos_src = ny * bytesperline_src + nx * channels_src;
						int val = datasrc[npos_src];

						if(val < min) min = val;
						if(val > max) max = val;
					}
				}
			}

			int threshold = (min + max) / 2;

			datadst[pos_dst] = (datasrc[pos_src] >= threshold) ? 255 : 0;
		}
	}

	return 1;
}

int vc_gray_to_binary_bernsen(IVC *src, IVC *dst, int kernel){

	unsigned char *datasrc = (unsigned char *) src->data;
	int bytesperline_src = src->width * src->channels;
	int channels_src = src->channels;
	unsigned char *datadst = (unsigned char *) dst->data;
	int bytesperline_dst = dst->width * dst->channels;
	int channels_dst = dst->channels;
	int width = src->width;
	int height = src->height;
	int x, y, kx, ky;
	long int pos_src, pos_dst;

	if((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if((src->width != dst->width) || (src->height != dst->height)) return 0;
	if((src->channels != 1) || (dst->channels != 1)) return 0;

	for(y=0; y<height; y++) {
		for(x=0; x<width; x++) {
			pos_src = y * bytesperline_src + x * channels_src;
			pos_dst = y * bytesperline_dst + x * channels_dst;

			int min = 255;
			int max = 0;

			for(ky=-kernel/2; ky<=kernel/2; ky++) {
				for(kx=-kernel/2; kx<=kernel/2; kx++) {
					int nx = x + kx;
					int ny = y + ky;

					if(nx >= 0 && nx < width && ny >= 0 && ny < height) {
						long int npos_src = ny * bytesperline_src + nx * channels_src;
						int val = datasrc[npos_src];

						if(val < min) min = val;
						if(val > max) max = val;
					}
				}
			}

			int contrast = max - min;

			if(contrast < 15) {
				datadst[pos_dst] = (max < 128) ? 255 : 0; 
			} else {
				int threshold = (min + max) / 2; 
				datadst[pos_dst] = (datasrc[pos_src] >= threshold) ? 255 : 0;
			}
		}
	}

	return 1;
}

int vc_binary_dilate(IVC *src, IVC *dst, int kernel){
	unsigned char *datasrc = (unsigned char *) src->data;
	int bytesperline_src = src->width * src->channels;
	int channels_src = src->channels;
	unsigned char *datadst = (unsigned char *) dst->data;
	int bytesperline_dst = dst->width * dst->channels;
	int channels_dst = dst->channels;
	int width = src->width;
	int height = src->height;
	int x, y, kx, ky;
	long int pos_src, pos_dst;

	if((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if((src->width != dst->width) || (src->height != dst->height)) return 0;
	if((src->channels != 1) || (dst->channels != 1)) return 0;

	for(y=0; y<height; y++) {
		for(x=0; x<width; x++) {
			pos_src = y * bytesperline_src + x * channels_src;
			pos_dst = y * bytesperline_dst + x * channels_dst;

			if(datasrc[pos_src] == 255) {
				datadst[pos_dst] = 255;

				for(ky=-kernel/2; ky<=kernel/2; ky++) {
					for(kx=-kernel/2; kx<=kernel/2; kx++) {
						int nx = x + kx;
						int ny = y + ky;

						if(nx >= 0 && nx < width && ny >= 0 && ny < height) {
							long int npos_dst = ny * bytesperline_dst + nx * channels_dst;
							datadst[npos_dst] = 255; 
						}
					}
				}
			} else {
				datadst[pos_dst] = 0;
			}
		}
	}

	return 1;
}

int vc_binary_erode(IVC *src, IVC *dst, int kernel){
	unsigned char *datasrc = (unsigned char *) src->data;
	int bytesperline_src = src->width * src->channels;
	int channels_src = src->channels;
	unsigned char *datadst = (unsigned char *) dst->data;
	int bytesperline_dst = dst->width * dst->channels;
	int channels_dst = dst->channels;
	int width = src->width;
	int height = src->height;
	int x, y, kx, ky;
	long int pos_src, pos_dst;

	if((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if((src->width != dst->width) || (src->height != dst->height)) return 0;
	if((src->channels != 1) || (dst->channels != 1)) return 0;

	for(y=0; y<height; y++) {
		for(x=0; x<width; x++) {
			pos_src = y * bytesperline_src + x * channels_src;
			pos_dst = y * bytesperline_dst + x * channels_dst;

			if(datasrc[pos_src] == 255) {
				int should_erode = 0;

				for(ky=-kernel/2; ky<=kernel/2 && !should_erode; ky++) {
					for(kx=-kernel/2; kx<=kernel/2 && !should_erode; kx++) {
						int nx = x + kx;
						int ny = y + ky;

						if(nx >= 0 && nx < width && ny >= 0 && ny < height) {
							long int npos_src = ny * bytesperline_src + nx * channels_src;

							if(datasrc[npos_src] == 0) {
								should_erode = 1; 
							}
						}
					}
				}

				datadst[pos_dst] = should_erode ? 0 : 255; 
			} else {
				datadst[pos_dst] = 0;
			}
		}
	}

	return 1;
}


// Remove ruido pequeno com morfologia
int vc_binary_open(IVC *src, IVC *dst, int kernel, int kernel2){
	IVC *temp = vc_image_new(src->width, src->height, src->channels, src->levels);
	if(temp == NULL) return 0;

	if(!vc_binary_erode(src, temp, kernel)) {
		vc_image_free(temp);
		return 0;
	}

	if(!vc_binary_dilate(temp, dst, kernel2)) {
		vc_image_free(temp);
		return 0;
	}

	vc_image_free(temp);
	return 1;

}


// Fecha buracos na segmentacao
int vc_binary_close(IVC *src, IVC *dst, int kernel, int kernel2){
	IVC *temp = vc_image_new(src->width, src->height, src->channels, src->levels);
	if(temp == NULL) return 0;

	if(!vc_binary_dilate(src, temp, kernel)) {
		vc_image_free(temp);
		return 0;
	}

	if(!vc_binary_erode(temp, dst, kernel2)) {
		vc_image_free(temp);
		return 0;
	}

	vc_image_free(temp);
	return 1;

}

int vc_erode_minus_dilate(IVC *img1, IVC *img2, IVC *dst){
    unsigned char *data1 = (unsigned char *) img1->data;
    unsigned char *data2 = (unsigned char *) img2->data;
    unsigned char *datadst = (unsigned char *) dst->data;

    int width = img1->width;
    int height = img1->height;
    int channels = img1->channels;

    for(int i = 0; i < width * height * channels; i++) {
        datadst[i] = (data1[i] > data2[i]) ? 255 : 0;
    }

    return 1;
}

int vc_gray_mask(IVC *src, IVC *mask, IVC *dst)
{
    unsigned char *datasrc = src->data;
    unsigned char *datamask = mask->data;
    unsigned char *datadst = dst->data;

    int width = src->width;
    int height = src->height;
    int bpl = src->bytesperline;

    long int pos;
    int x, y;

    if (!src || !mask || !dst) return 0;
    if (!datasrc || !datamask || !datadst) return 0;
    if (src->channels != 1 || mask->channels != 1 || dst->channels != 1) return 0;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pos = y * bpl + x;

            if (datamask[pos] == 255)
                datadst[pos] = datasrc[pos]; 
            else
                datadst[pos] = 0;
        }
    }

    return 1;
}


// Etiqueta os blobs da imagem binaria
OVC* vc_binary_blob_labelling(IVC *src, IVC *dst, int *nlabels)
{
	unsigned char *datasrc = (unsigned char *)src->data;
	unsigned char *datadst = (unsigned char *)dst->data;
	int width = src->width;
	int height = src->height;
	int bytesperline = src->bytesperline;
	int channels = src->channels;
	int x, y, a, b;
	long int i, size;
	long int posX, posA, posB, posC, posD;
	int labeltable[256] = { 0 };
	int labelarea[256] = { 0 };
	int label = 1;
	int num, tmplabel;
	OVC *blobs;

	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if ((src->width != dst->width) || (src->height != dst->height) || (src->channels != dst->channels)) return NULL;
	if (channels != 1) return NULL;

	memcpy(datadst, datasrc, bytesperline * height);

	for (i = 0, size = bytesperline * height; i<size; i++)
	{
		if (datadst[i] != 0) datadst[i] = 255;
	}

	for (y = 0; y<height; y++)
	{
		datadst[y * bytesperline + 0 * channels] = 0;
		datadst[y * bytesperline + (width - 1) * channels] = 0;
	}
	for (x = 0; x<width; x++)
	{
		datadst[0 * bytesperline + x * channels] = 0;
		datadst[(height - 1) * bytesperline + x * channels] = 0;
	}

	for (y = 1; y<height - 1; y++)
	{
		for (x = 1; x<width - 1; x++)
		{

			posA = (y - 1) * bytesperline + (x - 1) * channels;
			posB = (y - 1) * bytesperline + x * channels;
			posC = (y - 1) * bytesperline + (x + 1) * channels;
			posD = y * bytesperline + (x - 1) * channels;
			posX = y * bytesperline + x * channels;

			if (datadst[posX] != 0)
			{
				if ((datadst[posA] == 0) && (datadst[posB] == 0) && (datadst[posC] == 0) && (datadst[posD] == 0))
				{
					datadst[posX] = label;
					labeltable[label] = label;
					label++;
				}
				else
				{
					num = 255;

					if (datadst[posA] != 0) num = labeltable[datadst[posA]];
					if ((datadst[posB] != 0) && (labeltable[datadst[posB]] < num)) num = labeltable[datadst[posB]];
					if ((datadst[posC] != 0) && (labeltable[datadst[posC]] < num)) num = labeltable[datadst[posC]];
					if ((datadst[posD] != 0) && (labeltable[datadst[posD]] < num)) num = labeltable[datadst[posD]];

					datadst[posX] = num;
					labeltable[num] = num;

					if (datadst[posA] != 0)
					{
						if (labeltable[datadst[posA]] != num)
						{
							for (tmplabel = labeltable[datadst[posA]], a = 1; a<label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
					if (datadst[posB] != 0)
					{
						if (labeltable[datadst[posB]] != num)
						{
							for (tmplabel = labeltable[datadst[posB]], a = 1; a<label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
					if (datadst[posC] != 0)
					{
						if (labeltable[datadst[posC]] != num)
						{
							for (tmplabel = labeltable[datadst[posC]], a = 1; a<label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
					if (datadst[posD] != 0)
					{
						if (labeltable[datadst[posD]] != num)
						{
							for (tmplabel = labeltable[datadst[posD]], a = 1; a<label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
				}
			}
		}
	}

	for (y = 1; y<height - 1; y++)
	{
		for (x = 1; x<width - 1; x++)
		{
			posX = y * bytesperline + x * channels;

			if (datadst[posX] != 0)
			{
				datadst[posX] = labeltable[datadst[posX]];
			}
		}
	}

	for (a = 1; a<label - 1; a++)
	{
		for (b = a + 1; b<label; b++)
		{
			if (labeltable[a] == labeltable[b]) labeltable[b] = 0;
		}
	}
	*nlabels = 0;
	for (a = 1; a<label; a++)
	{
		if (labeltable[a] != 0)
		{
			labeltable[*nlabels] = labeltable[a];
			(*nlabels)++;
		}
	}

	if (*nlabels == 0) return NULL;

	blobs = (OVC *)calloc((*nlabels), sizeof(OVC));
	if (blobs != NULL)
	{
		for (a = 0; a<(*nlabels); a++) blobs[a].label = labeltable[a];
	}
	else return NULL;

	return blobs;
}


// Calcula area, perimetro, bounding box e centro
int vc_binary_blob_info(IVC *src, OVC *blobs, int nblobs)
{
	unsigned char *data = (unsigned char *)src->data;
	int width = src->width;
	int height = src->height;
	int bytesperline = src->bytesperline;
	int channels = src->channels;
	int x, y, i;
	long int pos;
	int xmin, ymin, xmax, ymax;
	long int sumx, sumy;

	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if (channels != 1) return 0;

	for (i = 0; i<nblobs; i++)
	{
		xmin = width - 1;
		ymin = height - 1;
		xmax = 0;
		ymax = 0;

		sumx = 0;
		sumy = 0;

		blobs[i].area = 0;
		blobs[i].perimeter = 0;

		for (y = 1; y<height - 1; y++)
		{
			for (x = 1; x<width - 1; x++)
			{
				pos = y * bytesperline + x * channels;

				if (data[pos] == blobs[i].label)
				{
					blobs[i].area++;

					sumx += x;
					sumy += y;

					if (xmin > x) xmin = x;
					if (ymin > y) ymin = y;
					if (xmax < x) xmax = x;
					if (ymax < y) ymax = y;

					if ((data[pos - 1] != blobs[i].label) || (data[pos + 1] != blobs[i].label) || (data[pos - bytesperline] != blobs[i].label) || (data[pos + bytesperline] != blobs[i].label))
					{
						blobs[i].perimeter++;
					}
				}
			}
		}

		blobs[i].x = xmin;
		blobs[i].y = ymin;
		blobs[i].width = (xmax - xmin) + 1;
		blobs[i].height = (ymax - ymin) + 1;

		blobs[i].xc = sumx / MAX(blobs[i].area, 1);
		blobs[i].yc = sumy / MAX(blobs[i].area, 1);
	}

	return 1;
}

int vc_histogram_image(IVC *src, IVC *dst)
{
    int histogram[256] = {0}; 
    int max_value = 0;
    long int pos;
    int x, y, bar;

    if (src == NULL || dst == NULL || src->data == NULL || dst->data == NULL) return 0;
    if (src->channels != 1 || dst->channels != 1) return 0;

    for (y = 0; y < src->height; y++)
    {
        for (x = 0; x < src->width; x++)
        {
            pos = y * src->bytesperline + x; 
            histogram[src->data[pos]]++;
        }
    }

    for (x = 0; x < 256; x++)
    {
        if (histogram[x] > max_value) max_value = histogram[x];
    }

    for (y = 0; y < dst->height; y++)
    {
        for (x = 0; x < dst->width; x++)
        {
            dst->data[y * dst->bytesperline + x] = 0;
        }
    }

    for (x = 0; x < 256 && x < dst->width; x++)
    {
        bar = (max_value > 0) ? (histogram[x] * dst->height) / max_value : 0;
        
        for (y = dst->height - 1; y >= dst->height - bar; y--)
        {
            dst->data[y * dst->bytesperline + x] = 255; 
        }
    }

    return 1;
}

int vc_histogram_equalization(IVC *src, IVC *dst)
{
	int histogram[256] = {0}; 
	int cdf[256] = {0}; 
	long int pos;
	int x, y;

	if (src == NULL || dst == NULL || src->data == NULL || dst->data == NULL) return 0;
	if (src->channels != 1 || dst->channels != 1) return 0;

	for (y = 0; y < src->height; y++)
	{
		for (x = 0; x < src->width; x++)
		{
			pos = y * src->bytesperline + x; 
			histogram[src->data[pos]]++;
		}
	}

	cdf[0] = histogram[0];
	for (x = 1; x < 256; x++)
	{
		cdf[x] = cdf[x - 1] + histogram[x];
	}

	int total_pixels = src->width * src->height;
	for (y = 0; y < dst->height; y++)
	{
		for (x = 0; x < dst->width; x++)
		{
			pos = y * dst->bytesperline + x;
			dst->data[pos] = (unsigned char)((cdf[src->data[pos]] * 255) / total_pixels);
		}
	}

	return 1;
}

int vc_gray_edge_prewitt(IVC *src, IVC *dst, float th){
	unsigned char *datasrc = (unsigned char *) src->data;
	int bytesperline_src = src->width * src->channels;
	int channels_src = src->channels;
	unsigned char *datadst = (unsigned char *) dst->data;
	int bytesperline_dst = dst->width * dst->channels;
	int channels_dst = dst->channels;
	int width = src->width;
	int height = src->height;
	int x, y, kx, ky;
	long int pos_src, pos_dst;

	if((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if((src->width != dst->width) || (src->height != dst->height)) return 0;
	if((src->channels != 1) || (dst->channels != 1)) return 0;

	for(y=1; y<height-1; y++) {
		for(x=1; x<width-1; x++) {
			pos_src = y * bytesperline_src + x * channels_src;
			pos_dst = y * bytesperline_dst + x * channels_dst;

			float gx = -datasrc[(y-1)*bytesperline_src + (x-1)*channels_src] -
			 datasrc[y*bytesperline_src + (x-1)*channels_src] - 
			 datasrc[(y+1)*bytesperline_src + (x-1)*channels_src]
						+ datasrc[(y-1)*bytesperline_src + (x+1)*channels_src] + 
						datasrc[y*bytesperline_src + (x+1)*channels_src] + 
						datasrc[(y+1)*bytesperline_src + (x+1)*channels_src];

			float gy = -datasrc[(y-1)*bytesperline_src + (x-1)*channels_src] - datasrc[(y-1)*bytesperline_src + x*channels_src] - datasrc[(y-1)*bytesperline_src + (x+1)*channels_src]
						+ datasrc[(y+1)*bytesperline_src + (x-1)*channels_src] + datasrc[(y+1)*bytesperline_src + x*channels_src] + datasrc[(y+1)*bytesperline_src + (x+1)*channels_src];

			float magnitude = sqrtf(gx * gx + gy * gy);
			datadst[pos_dst] = (magnitude >= th) ? 255 : 0;
		}
	}

}

int vc_gray_lowpass_mean_filter(IVC *src, IVC *dst, int kernelsize){
	unsigned char *datasrc = (unsigned char *) src->data;
	int bytesperline_src = src->width * src->channels;
	int channels_src = src->channels;
	unsigned char *datadst = (unsigned char *) dst->data;
	int bytesperline_dst = dst->width * dst->channels;
	int channels_dst = dst->channels;
	int width = src->width;
	int height = src->height;
	int x, y, kx, ky;
	long int pos_src, pos_dst;

	if((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if((src->width != dst->width) || (src->height != dst->height)) return 0;
	if((src->channels != 1) || (dst->channels != 1)) return 0;

	for(y=0; y<height; y++) {
		for(x=0; x<width; x++) {
			pos_src = y * bytesperline_src + x * channels_src;
			pos_dst = y * bytesperline_dst + x * channels_dst;

			int sum = 0;
			int count = 0;

			for(ky=-kernelsize/2; ky<=kernelsize/2; ky++) {
				for(kx=-kernelsize/2; kx<=kernelsize/2; kx++) {
					int nx = x + kx;
					int ny = y + ky;

					if(nx >= 0 && nx < width && ny >= 0 && ny < height) {
						long int npos_src = ny * bytesperline_src + nx * channels_src;
						sum += datasrc[npos_src];
						count++;
					}
				}
			}

			datadst[pos_dst] = (unsigned char)(sum / count);
		}
	}

	return 1;
}

	int vc_gray_lowpass_median_filter(IVC *src, IVC *dst, int kernelsize)
{
    if (!src || !dst || !src->data || !dst->data) return 0;
    if (src->width != dst->width || src->height != dst->height) return 0;
    if (src->channels != 1 || dst->channels != 1) return 0;

    int width = src->width;
    int height = src->height;
    int bytesperline = width * src->channels;

    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;

    int x, y, kx, ky;

    int window_size = kernelsize * kernelsize;
    unsigned char *window = (unsigned char *)malloc(window_size);

    if (!window) return 0;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {

            int count = 0;

            for (ky = -kernelsize / 2; ky <= kernelsize / 2; ky++) {
                for (kx = -kernelsize / 2; kx <= kernelsize / 2; kx++) {

                    int nx = x + kx;
                    int ny = y + ky;

                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        int pos = ny * bytesperline + nx;
                        window[count++] = datasrc[pos];
                    }
                }
            }

            for (int i = 0; i < count - 1; i++) {
                for (int j = i + 1; j < count; j++) {
                    if (window[i] > window[j]) {
                        unsigned char temp = window[i];
                        window[i] = window[j];
                        window[j] = temp;
                    }
                }
            }

            datadst[y * bytesperline + x] = window[count / 2];
        }
    }

    free(window);

    return 1;
}

int vc_gray_lowpass_gaussian_filter(IVC *src, IVC *dst){
	unsigned char *datasrc = (unsigned char *) src->data;
	int bytesperline_src = src->width * src->channels;
	int channels_src = src->channels;
	unsigned char *datadst = (unsigned char *) dst->data;
	int bytesperline_dst = dst->width * dst->channels;
	int channels_dst = dst->channels;
	int width = src->width;
	int height = src->height;
	int x, y, kx, ky;
	long int pos_src, pos_dst;

	if((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if((src->width != dst->width) || (src->height != dst->height)) return 0;
	if((src->channels != 1) || (dst->channels != 1)) return 0;

	float sigma = 1.0f;

	for(y=0; y<height; y++) {
		for(x=0; x<width; x++) {

			pos_dst = y * bytesperline_dst + x * channels_dst;

			float sum = 0.0f;
			float weight_sum = 0.0f;

			for(ky=-1; ky<=1; ky++) {
				for(kx=-1; kx<=1; kx++) {
					int nx = x + kx;
					int ny = y + ky;

					if(nx >= 0 && nx < width && ny >= 0 && ny < height) {
						long int npos_src = ny * bytesperline_src + nx * channels_src;

						float weight = expf(-(kx*kx + ky*ky) / (2.0f * sigma * sigma));

						sum += datasrc[npos_src] * weight;
						weight_sum += weight;
					}
				}
			}

			datadst[pos_dst] = (unsigned char)(sum / weight_sum + 0.5f);
		}
	}

	return 1;
}
