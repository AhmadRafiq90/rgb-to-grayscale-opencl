// Kernel function to convert RGB image to grayscale
__kernel void rgb_to_gray(__global uchar* input_image,
                          __global uchar* output_image,
                          const int width,
                          const int height) {
    // Get the global ID (pixel index)
    int global_id = get_global_id(0);
    
    // Make sure the global ID is within the bounds of the image
    if (global_id < width * height) {
        // Calculate the row and column of the pixel
        int row = global_id / width;
        int col = global_id % width;

        // Get the RGB values of the current pixel from the input image
        uchar red = input_image[row + width * (col + 3 * 0)];
        uchar green = input_image[row + width * (col + 3 * 1)];
        uchar blue = input_image[row + width * (col + 3 * 2)];

        // Calculate the grayscale intensity using the luminosity method
        uchar gray = 0.21 * red + 0.72 * green + 0.07 * blue;

        // Store the grayscale intensity in the output image
        // Translating 2d address to 1d address
        output_image[(row * width) + col] = gray;
    }
}
