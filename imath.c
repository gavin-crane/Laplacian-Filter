//Project 2, laplacian filter image
//Group members: David Lee, Gavin Crane
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>

#define THREADS 4
#define FILTER_WIDTH 3
#define FILTER_HEIGHT 3

#define RGB_MAX 255

typedef struct
{
    unsigned char r, g, b;
} PPMPixel;

struct parameter
{
    PPMPixel *image;         // original image
    PPMPixel *result;        // filtered image
    unsigned long int w;     // width of image
    unsigned long int h;     // height of image
    unsigned long int start; // starting point of work
    unsigned long int size;  // equal share of work (almost equal if odd)
};

/*This is the thread function. It will compute the new values for the region of image specified in params (start to start+size) using convolution.
    (1) For each pixel in the input image, the filter is conceptually placed on top ofthe image with its origin lying on that pixel.
    (2) The  values  of  each  input  image  pixel  under  the  mask  are  multiplied  by the corresponding filter values.
    (3) The results are summed together to yield a single output value that is placed in the output image at the location of the pixel being processed on the input.

 */
// Color balancer helps with keeping the color range between 0-255
int color_balancer(int val)
{
    if (val < 0)
    {
        val = 0;
    }
    if (val > RGB_MAX)
    {
        val = RGB_MAX;
    }
    return val;
}

void *threadfn(void *params)
{
    // laplacian filter
    int laplacian[FILTER_WIDTH][FILTER_HEIGHT] = {{-1, -1, -1}, {-1, 8, -1}, {-1, -1, -1}};

    struct parameter *input = (struct parameter *)params;
    PPMPixel *image = input->image;
    PPMPixel *result = input->result;
    unsigned long int imageWidth = input->w, imageHeight = input->h;

    // first 2 loop to iterate the image
    for (int iteratorImageHeight = input->start; iteratorImageHeight < input->size + input->start; iteratorImageHeight++)
    {
        for (int iteratorImageWidth = 0; iteratorImageWidth < input->w; iteratorImageWidth++)
        {

            int red = 0, green = 0, blue = 0, x_coordinate = 0, y_coordinate = 0;

            // following 2 loop to iterate the filter
            for (int iteratorFilterHeight = 0; iteratorFilterHeight < FILTER_HEIGHT; iteratorFilterHeight++)
            {
                for (int iteratorFilterWidth = 0; iteratorFilterWidth < FILTER_WIDTH; iteratorFilterWidth++)
                {

                    // threadfn function: use these equations to do the math of image filtering
                    x_coordinate = (iteratorImageWidth - FILTER_WIDTH / 2 + iteratorFilterWidth + imageWidth) % imageWidth;
                    y_coordinate = (iteratorImageHeight - FILTER_HEIGHT / 2 + iteratorFilterHeight + imageHeight) % imageHeight;
                    red += image[y_coordinate * imageWidth + x_coordinate].r * laplacian[iteratorFilterHeight][iteratorFilterWidth];
                    green += image[y_coordinate * imageWidth + x_coordinate].g * laplacian[iteratorFilterHeight][iteratorFilterWidth];
                    blue += image[y_coordinate * imageWidth + x_coordinate].b * laplacian[iteratorFilterHeight][iteratorFilterWidth];
                    // red, green and blue must not be of type char to avoid wrapping.
                }
            }
            // color balancing after applying filter
            red = color_balancer(red);
            green = color_balancer(green);
            blue = color_balancer(blue);

            // store filtered image into result
            result[iteratorImageHeight * imageWidth + iteratorImageWidth].r = red;
            result[iteratorImageHeight * imageWidth + iteratorImageWidth].g = green;
            result[iteratorImageHeight * imageWidth + iteratorImageWidth].b = blue;
        }
    }

    return NULL;
}

// function to write ppm file
void writeImage(PPMPixel *image, char *name, unsigned long int width, unsigned long int height)
{
    FILE *writeout;
    writeout = fopen(name, "wb");
    if (!writeout)
    {
        fprintf(stderr, "Unable to open file %s\n", name);
        exit(1);
    }

    const char *comment = "# CREATOR: The Awesome team 9000";
    fprintf(writeout, "P6\n%s\n%lu %lu\n%d\n", comment, width, height, RGB_MAX);

    fwrite(image, 3 * width, height, writeout);
    fclose(writeout);
}

// Open the filename image for reading, and parse it.
PPMPixel *readImage(const char *filename, unsigned long int *width, unsigned long int *height)
{
    PPMPixel *img;

    char dimensions[64];
    FILE *photo;

    photo = fopen(filename, "rb");

    if (!photo)
    {
        printf("Unable to read file \n");
        exit(1);
    }

    // first fgets check image format
    fgets(dimensions, 512, photo);
    if (strcmp(dimensions, "P6\n") != 0)
    {
        printf("Image format is not P6\n");
        exit(1);
    }

    // 2nd fgets ignored
    fgets(dimensions, sizeof(dimensions), photo);

    // 3rd fgets get dimensions
    if (fscanf(photo, "%lu %lu", width, height) != 2)
    {
        fprintf(stderr, "Invalid image size (error loading '%s')\n", filename);
        exit(1);
    }
    fgets(dimensions, sizeof(dimensions), photo);

    // 4th fgets check image color range
    fgets(dimensions, sizeof(dimensions), photo);
    
    // printf("%s", dimensions);
    if (strcmp(dimensions, "255\n") != 0)
    {
        printf("Image color value is not 255\n");
        exit(1);
    }
    int w = *width;
    int h = *height;

    img = (PPMPixel *)malloc(3 * w * h * sizeof(PPMPixel));

    if (!img)
    {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    if (fread(img, 3 * w, w * h, photo) != h)
    {
        fprintf(stderr, "Error loading image '%s'\n", filename);
        exit(1);
    }

    fclose(photo);
    return img;
}

// Create threads and apply filter to image.
PPMPixel *apply_filters(PPMPixel *image, unsigned long w, unsigned long h, double *elapsedTime)
{
    pthread_t th[THREADS];
    
    struct timeval start, end;
    gettimeofday(&start, NULL);

    // allocate memory for result
    PPMPixel *result;
    result = (PPMPixel *)malloc(3 * w * h * sizeof(PPMPixel));
    
    // determine size of work for each thread
    unsigned long size = h / THREADS;
    printf("number of threads: %d\n", THREADS);

    // create a parameter for each thread
    struct parameter *threadParams = malloc(sizeof(struct parameter) * THREADS);

    // create each thread and fill its params and call threadfn
    int i; 
    for (i = 0; i < THREADS; i++)
    {
        threadParams[i].image = image;
        threadParams[i].result = result;
        threadParams[i].w = w;
        threadParams[i].h = h;
        threadParams[i].start = i * size;

        // for last thread make sure it does the rest of the work if size does not divide evenly
        if (i == THREADS - 1)
        {
            threadParams[i].size = h - (i * size);
        }
        else
        {
            threadParams[i].size = (i * size) + size;
        }
        if (pthread_create(&th[i], NULL, &threadfn, (void *)&threadParams[i]) != 0)
        {
            printf("error creating thread");
            exit(1);
        }
    }
    
    // join the threads and end execution
    for (i = 0; i < THREADS; i++)
    {
        if (pthread_join(th[i], NULL) != 0)
        {
            printf("error joining thread");
        }
    }
    free(threadParams);
    
    // end timer
    gettimeofday(&end, NULL);
    double seconds = (end.tv_sec - start.tv_sec);
    double micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);
    *elapsedTime = micros;

    return result;
}

/*The driver of the program. Check for the correct number of arguments. If wrong print the message: "Usage ./a.out filename"
    Read the image that is passed as an argument at runtime. Apply the filter. Print elapsed time in .3 precision (e.g. 0.006 s). Save the result image in a file called laplacian.ppm. Free allocated memory.
 */
int main(int argc, char *argv[])
{
    unsigned long w, h;
    PPMPixel *image, *result;
    if (argc > 2)
    {
        printf("Too few arguments");
        exit(0);
    }
    if (argc > 3)
    {
        printf("Too many arguments");
        exit(0);
    }

    // read and apply filter, keeping track of time
    image = readImage(argv[1], &w, &h);
    double elapsedTime = 0.0;
    result = apply_filters(image, w, h, &elapsedTime);

    // format output file name and write filtered image to file
    char outputFile[128];
    strcpy(outputFile, argv[1]);
    strtok(outputFile, ".");
    strcat(outputFile, "_laplacian.ppm");
    writeImage(result, outputFile, w, h);
    
    printf("the time taken is %f microseconds \n", elapsedTime);
    free(image);
    free(result);
    return 0;
}
