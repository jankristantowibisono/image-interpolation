#include<iostream>
#include<fstream>
#include<string>
#include<windows.h>
#include<Strsafe.h>
using namespace std;

bool SaveBitmapToFile(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD wBitsPerPixel, LPCTSTR lpszFiLENAme){
	RGBQUAD palette[256];
	for (int i = 0; i < 256; ++i){
		palette[i].rgbBlue = (byte)i;
		palette[i].rgbGreen = (byte)i;
		palette[i].rgbRed = (byte)i;
	}

	BITMAPINFOHEADER bmpInfoHeader = { 0 };
	bmpInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmpInfoHeader.biBitCount = wBitsPerPixel;
	bmpInfoHeader.biClrImportant = 0;
	bmpInfoHeader.biClrUsed = 0;
	bmpInfoHeader.biCompression = BI_RGB;
	bmpInfoHeader.biHeight = -lHeight;
	bmpInfoHeader.biWidth = lWidth;
	bmpInfoHeader.biPlanes = 1;
	bmpInfoHeader.biSizeImage = lWidth* lHeight * (wBitsPerPixel / 8);

	BITMAPFILEHEADER bfh = { 0 };
	// This value should be values of BM letters i.e 0x4D42
	bfh.bfType = 0x4d42;
	// Offset to the RGBQUAD
	bfh.bfOffBits = sizeof(BITMAPINFOHEADER)+sizeof(BITMAPFILEHEADER)+sizeof(RGBQUAD)* 256;
	// Total size of image including size of headers
	bfh.bfSize = bfh.bfOffBits + bmpInfoHeader.biSizeImage;
	
	HANDLE hFile = CreateFile(lpszFiLENAme, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (!hFile) {
		// return if error opening file
		return 0;
	}

	DWORD dwWritten = 0;
	// Write the File header
	WriteFile(hFile, &bfh, sizeof(bfh), &dwWritten, NULL);
	// Write the bitmap info header
	WriteFile(hFile, &bmpInfoHeader, sizeof(bmpInfoHeader), &dwWritten, NULL);
	// Write the palette
	WriteFile(hFile, &palette[0], sizeof(RGBQUAD)* 256, &dwWritten, NULL);
	// Write the RGB Data
	if (lWidth % 4 == 0)
	{
		WriteFile(hFile, pBitmapBits, bmpInfoHeader.biSizeImage, &dwWritten, NULL);
	}
	else
	{
		char* empty = new char[4 - lWidth % 4];
		for (int i = 0; i < lHeight; ++i)
		{
			WriteFile(hFile, &pBitmapBits[i * lWidth], lWidth, &dwWritten, NULL);
			WriteFile(hFile, empty, 4 - lWidth % 4, &dwWritten, NULL);
		}
	}
	// Close the file handleWrite
	CloseHandle(hFile);
	return 1;
}

BYTE* SaveBitmapDownSampling(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD rasio, LPCTSTR lpszFiLENAme){
	BYTE * buffer;
	LONG neww = lWidth / rasio;
	buffer = new BYTE[neww * neww];
	int x = 0;
	for (int i = 0; i<(neww * neww); i++){
		buffer[i] = pBitmapBits[x * lWidth * rasio + i*rasio];
		if (i % (lWidth) == 0) x++;
	}
	SaveBitmapToFile(buffer, lWidth/rasio , lHeight/rasio , 8, lpszFiLENAme);
	return buffer;
}

BYTE* resizePixels(BYTE* pixels, int w1, int h1, int w2, int h2) {
	BYTE * temp;
	temp = new BYTE[w2 * h2];
	double x_ratio = w1 / (double)w2;
	double y_ratio = h1 / (double)h2;
	double px, py;
	for (int i = 0; i<h2; i++) {
		for (int j = 0; j<w2; j++) {
			px = floor(j*x_ratio);
			py = floor(i*y_ratio);
			temp[(i*w2) + j] = pixels[(int)((py*w1) + px)];
		}
	}
	return temp;
}

BYTE* biLinear(BYTE*buffer,int w, int h, int w2, int h2){
	BYTE * temp;
	temp = new BYTE[w2 * h2];
	int A, B, C, D, x, y, index, gray;

	float x_ratio = ((float)(w - 1)) / w2;
	float y_ratio = ((float)(h - 1)) / h2;
	float x_diff, y_diff;
	int offset = 0;
	for (int i = 0; i<h2; i++) {
		for (int j = 0; j<w2; j++) {
			x = (int)(x_ratio * j);
			y = (int)(y_ratio * i);
			x_diff = (x_ratio * j) - x;
			y_diff = (y_ratio * i) - y;
			index = y*w + x;
			// range is 0 to 255 thus bitwise AND with 0xff
			A = buffer[index] & 0xff;
			B = buffer[index + 1] & 0xff;
			C = buffer[index + w] & 0xff;
			D = buffer[index + w + 1] & 0xff;
			// Y = A(1-w)(1-h) + B(w)(1-h) + C(h)(1-w) + Dwh
			gray = (int)(A*(1 - x_diff)*(1 - y_diff) + B*(x_diff)*(1 - y_diff) +
				C*(y_diff)*(1 - x_diff) + D*(x_diff*y_diff));
			temp[offset++] = gray;
		}
	}
	return temp;
}

BYTE getpixel(BYTE* in, int src_width, int src_height, int x, int y, int channel)
{
	// implement mirror padding
	if (x < 0){
		x = x * -1;
	}
	if (y < 0){
		y = y * -1;
	}
	if (x > src_width){
		x = src_width * 2 - x;
	}
	if (y > src_width){
		y = src_width * 2 - y;
	}

	if (x < src_width && y < src_height){
		return in[(x * src_width) + (y)];
	}
	return 0;
}
BYTE* bicubicresize2(BYTE* in, int src_width, int src_height, int dest_width, int dest_height)
{
	BYTE* out;
	out = new BYTE[dest_width * dest_height];

	const float tx = float(src_width) / dest_width;
	const float ty = float(src_height) / dest_height;
	const int channels = 1;
	const int row_stride = dest_width * channels;

	double C[5] = { 0 };

	for (int i = 0; i < dest_height; i++)
	{
		for (int j = 0; j < dest_width; j++)
		{
			const int x = int(tx * j);
			const int y = int(ty * i);
			const float dx = tx * j - x;
			const float dy = ty * i - y;

				for (int jj = 0; jj < 4; ++jj)
				{
					const int z = y - 1 + jj;
					double a0 = getpixel(in, src_width, src_height, z, x, channels);
					double d0 = getpixel(in, src_width, src_height, z, x - 1, channels) - a0;
					double d2 = getpixel(in, src_width, src_height, z, x + 1, channels) - a0;
					double d3 = getpixel(in, src_width, src_height, z, x + 2, channels) - a0;
					double a1 = -1.0 / 3 * d0 + d2 - 1.0 / 6 * d3;
					double a2 = 1.0 / 2 * d0 + 1.0 / 2 * d2;
					double a3 = -1.0 / 6 * d0 - 1.0 / 2 * d2 + 1.0 / 6 * d3;
					C[jj] = a0 + a1 * dx + a2 * dx * dx + a3 * dx * dx * dx;

					d0 = C[0] - C[1];
					d2 = C[2] - C[1];
					d3 = C[3] - C[1];
					a0 = C[1];
					a1 = -1.0 / 3 * d0 + d2 - 1.0 / 6 * d3;
					a2 = 1.0 / 2 * d0 + 1.0 / 2 * d2;
					a3 = -1.0 / 6 * d0 - 1.0 / 2 * d2 + 1.0 / 6 * d3;
					out[i * row_stride + j * channels + channels] = (BYTE)(a0 + a1 * dy + a2 * dy * dy + a3 * dy * dy * dy);
				}
		}
	}
	return out;
}

double mse(BYTE *in1,BYTE *in2, int size ){
	double selisih =0.0; // different
	double total =0.0; // total=mse 
	
	for (int i = 0; i < size*size; i++){
		selisih = (double)in1[i] - (double)in2[i];
		total += selisih*selisih;
	}
	
	return (double)(total / (size*size));
}

double psnr(double mse){
	return 10.0 * log10(255.0 * 255.0 / mse);
}

int main(){	
	
	HANDLE hFile;
	DWORD wmWritten;
	BYTE strVal[512*512];
	BYTE temp[512 * 512];
	string filen;
	//filen = "C:/Users/Jan/Documents/Visual Studio 2013/Projects/testVoid/Debug/LENA.raw";
	//hFile = CreateFile((LPCTSTR)filen.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	hFile = CreateFile(L"LENA.raw", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	ReadFile(hFile, temp, 512*512, &wmWritten, NULL);
	CloseHandle(hFile);

	/*for (int a = (512*512)-1; a>=0; a--){
		strVal[(512 * 512) - 1 - a] = temp[a];
	}*/
	 
	if (SaveBitmapToFile(temp, 512, 512, 8, L"LENA-ORIGINAL.bmp")){
		cout << "Generate Original 512 BMP Image successfully"<<"\n";
	}

	BYTE* t1 = resizePixels(temp, 512, 512, 256, 256);
	if (SaveBitmapToFile(t1, 256, 256, 8, L"LENA-ORIGINAL-TO-256.bmp")){
		cout << "Generate 256 BMP Image successfully" << "\n";
	}

	BYTE* t2 = resizePixels(t1, 256, 256, 128, 128);
	if (SaveBitmapToFile(t2, 128, 128, 8, L"LENA-256-TO-128.bmp")){
		cout << "Generate 128 BMP Image successfully" << "\n";
	}

	BYTE* t3 = resizePixels(t2, 128, 128, 256, 256);
	if (SaveBitmapToFile(t3, 256, 256, 8, L"LENA-128-TO-256-nearest.bmp")){
		cout << "Generate 256 BMP Image with nearest-neighbor successfully" << "\n";
	}

	BYTE* t4 = resizePixels(t2, 128, 128, 512, 512);
	if (SaveBitmapToFile(t4, 512, 512, 8, L"LENA-128-TO-512-nearest.bmp")){
		cout << "Generate 512 BMP Image with nearest-neighbor successfully" << "\n";
	}

	BYTE* t5 = biLinear(t2, 128, 128, 512, 512);
	if (SaveBitmapToFile(t5, 512, 512, 8, L"LENA-128-TO-512-bilinear.bmp")){
		cout << "Generate 512 BMP Image with bilinear successfully" << "\n";
	}

	BYTE* t6 = biLinear(t2, 128, 128, 256, 256);
	if (SaveBitmapToFile(t6, 256, 256, 8, L"LENA-128-TO-256-bilinear.bmp")){
		cout << "Generate 256 BMP Image with bilinear successfully" << "\n";
	}

	BYTE* t7 = bicubicresize2(t2, 128, 128, 512, 512);
	if (SaveBitmapToFile(t7, 512, 512, 8, L"LENA-128-TO-512-bicubic.bmp")){
		cout << "Generate 512 BMP Image with bicubic successfully" << "\n";
	}

	BYTE* t8 = bicubicresize2(t2, 128, 128, 256, 256);
	if (SaveBitmapToFile(t8, 256, 256, 8, L"LENA-128-TO-256-bicubic.bmp")){
		cout << "Generate 256 BMP Image with bicubic successfully" << "\n";
	}

	BYTE* t9 = bicubicresize2(t1, 256, 256, 512, 512);
	if (SaveBitmapToFile(t9, 512, 512, 8, L"LENA-256-TO-512-bicubic.bmp")){
		cout << "Generate 512 BMP Image with bicubic successfully" << "\n";
	}
	
	cout << "MSE 512 Bicubic : " << mse(temp, t7, 512) << "\n";
	cout << "PNSR 512 Bicubic : " << psnr(mse(temp, t7, 512)) << "\n";
	cout << "MSE 512 Bilinear : " << mse(temp, t5, 512) << "\n";
	cout << "PNSR 512 Bilinear : " << psnr(mse(temp, t5, 512)) << "\n";
	cout << "MSE 512 Nearest : " << mse(temp, t4, 512) << "\n";
	cout << "PNSR 512 Nearest : " << psnr(mse(temp, t4, 512)) << "\n";
	return 0;
}

//tipsandtricks.runicsoft.com/Cpp/BitmapTutorial.html
//tech-algorithm.com/articles/nearest-neighbor-image-scaling/
//tech-algorithm.com/articles/bilinear-image-scaling/

