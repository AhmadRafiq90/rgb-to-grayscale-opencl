// C standard includes
#include <iostream>
#include <CL/opencl.hpp>
#include <Windows.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <locale>
#include <codecvt>
#include <vector>
#include <fstream>
#include <omp.h>
using namespace std;
using namespace cv;
using namespace cl;
int counter = 0;

void get_images(string);
uchar* matToArray(const Mat&);
void free3DArray(uchar***, int, int);
uchar** allocate2DArray(int, int);
void deallocate2DArray(uchar**, int);
void executeOpenCL(uchar*, uchar*, const int, const int);
int translate3Dto1D(int, int, int, int, int);
void translate1Dto3D(int, int, int, int*, int*, int*);

int main()
{
    /*cl_int CL_err = CL_SUCCESS;
    cl_uint numPlatforms = 0;

    CL_err = clGetPlatformIDs(0, NULL, &numPlatforms);

    if (CL_err == CL_SUCCESS)
        printf_s("%u platform(s) found\n", numPlatforms);
    else
        printf_s("clGetPlatformIDs(%i)\n", CL_err);*/

        // Get all available platforms
 
    omp_set_num_threads(4);
    string folder_path = "D:/ISIC_2020_Test_JPEG/ISIC_2020_Test_Input";
    get_images(folder_path);

    return 0;
  
}

void get_images(string folder_path)
{
	std::wstring folder_path_wide(folder_path.begin(), folder_path.end());

	WIN32_FIND_DATA file_data;
	HANDLE hFind = FindFirstFileW((folder_path_wide + L"\\*").c_str(), &file_data);
    std::vector<string> files;
    wstring_convert<codecvt_utf8<wchar_t>, wchar_t> converter;

    if (hFind != INVALID_HANDLE_VALUE) 
    {
        do 
        {
            if (!(file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) 
            {
				wstring file_name = file_data.cFileName;
				wstring extension = file_name.substr(file_name.find_last_of(L".") + 1);
				transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                if (extension == L"jpg")
                {
                    files.push_back(folder_path + "/" + converter.to_bytes(file_name));
				}
			}
		} while (FindNextFileW(hFind, &file_data) != 0);
        FindClose(hFind);

        cout << "Total images found: " << files.size() << endl;

        for (int i = 0; i < files.size(); i++)
        {
            Mat colored_img = imread(files[i], IMREAD_COLOR);
            int rows = colored_img.rows, cols = colored_img.cols;
			uchar* input = matToArray(colored_img);
            colored_img.release();
			uchar* output = new uchar [rows * cols];
			executeOpenCL(input, output, rows, cols);
            delete[] input;
			delete[] output;
        }
        cout << "Done with all images" << endl;
	}
    else {
		std::cerr << "Error: Cannot open directory" << std::endl;
	}
}

uchar* matToArray(const cv::Mat& image) {
    int height = image.rows;
    int width = image.cols;
    const int size = height * width * 3;
    // Allocate memory for the 3D array
    uchar* array = new uchar [size];

    // Copy pixel values from the Mat object to the 3D array
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            cv::Vec3b pixel = image.at<cv::Vec3b>(i, j);
            array[translate3Dto1D(i, j, 0, width, height)] = pixel[0]; // Blue channel
            array[translate3Dto1D(i, j, 1, width, height)] = pixel[1]; // Green channel
            array[translate3Dto1D(i, j, 2, width, height)] = pixel[2]; // Red channel
        }
    }

    return array;
}

int translate3Dto1D(int x, int y, int z, int width, int height) {
    return x + width * (y + 3 * z);
}

void translate1Dto3D(int index, int width, int height, int* x, int* y, int* z) {
    *x = index / (width * height);
    *y = (index / height) % width;
    *z = index % height;
}

void executeOpenCL(uchar* input, uchar* output, const int height, const int width)
{
    // Initialize OpenCL
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);

    // Display available platforms and let the user choose
    std::cout << "Available OpenCL Platforms:" << std::endl;
    for (size_t i = 0; i < platforms.size(); ++i) {
        std::cout << i + 1 << ". " << platforms[i].getInfo<CL_PLATFORM_NAME>() << std::endl;
    }

    int selected_platform;
    std::cout << "Select a platform: ";
    std::cin >> selected_platform;

    if (selected_platform < 1 || selected_platform > platforms.size()) {
        std::cerr << "Invalid platform selection" << std::endl;
        return;
    }

    // Get the chosen platform
    cl::Platform platform = platforms[selected_platform - 1];

    // Get all available devices for the chosen platform
    std::vector<cl::Device> devices;
    platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);

    // Display available devices and let the user choose
    std::cout << "Available Devices:" << std::endl;
    for (size_t i = 0; i < devices.size(); ++i) {
        std::cout << i + 1 << ". " << devices[i].getInfo<CL_DEVICE_NAME>() << " "
            << devices[i].getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>() << std::endl;
    }

    int selected_device;
    std::cout << "Select a device (GPU/CPU): ";
    std::cin >> selected_device;

    if (selected_device < 1 || selected_device > devices.size()) {
        std::cerr << "Invalid device selection" << std::endl;
        return;
    }

    // Get the chosen device
    cl::Device device = devices[selected_device - 1];

    // Initialize OpenCL context and command queue
    cl::Context context(device);
    cl::CommandQueue queue(context, device);

    // Read the OpenCL kernel source code from a file
    std::ifstream kernel_file("kernel.cl");
    std::string kernel_code(std::istreambuf_iterator<char>(kernel_file), (std::istreambuf_iterator<char>()));

    // Create OpenCL program from the kernel source code
    cl::Program::Sources sources;
    sources.push_back({ kernel_code.c_str(), kernel_code.length() });
    cl::Program program(context, sources);
    cl_int err = program.build({ device });
    if (err != CL_SUCCESS) {
        std::cerr << "Error building program: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << std::endl;
        return ;
    }

    // Create OpenCL kernel object
    cl::Kernel kernel(program, "rgb_to_gray");
    if (sizeof(uchar) * width * height * 3 > device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>()) {
		std::cerr << "Image size exceeds device memory limit" << std::endl;
		return;
	}
    // Create OpenCL buffers for input and output image data
    cl::Buffer input_buffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(uchar) * width * height * 3, input, &err);
    cl::Buffer output_buffer(context, CL_MEM_WRITE_ONLY, sizeof(uchar) * width * height, nullptr, &err);

    // Set kernel arguments
    err |= kernel.setArg(0, input_buffer);
    err |= kernel.setArg(1, &output_buffer);
    err |= kernel.setArg(2, width);
    err |= kernel.setArg(3, height);

    // Enqueue kernel for execution
    cl::NDRange global_size(width * height);
    err = queue.enqueueNDRangeKernel(kernel, cl::NullRange, global_size);

    // Read back the output data from OpenCL buffer to host memory
    queue.enqueueReadBuffer(output_buffer, CL_TRUE, 0, sizeof(uchar) * width * height, output);

    // Process the output data as needed
    Mat processed_image(height, width, CV_8UC1);
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            processed_image.at<uchar>(i, j) = output[i * width + j];
        }
    }
    cv::imwrite("p" + to_string(counter++) + ".jpg", processed_image);
}