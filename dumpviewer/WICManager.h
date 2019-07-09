#pragma once

#include <stdexcept>
#include <vector>
#include <atlbase.h>
#include <wincodec.h>

class WICManager
{
public:
	using buffer_t = std::vector<uint8_t>;
	using failed_load = std::logic_error;

	bool Create();
	void Release();

	buffer_t Load( const void *image, size_t image_size );
	buffer_t Load( PCWSTR uri );
	void SaveToPng( const LPCTSTR path, const void *pixel_data, int width, int height );

private:
	CComPtr<IWICImagingFactory> m_Factory;

	CComPtr<IWICBitmapDecoder> CreateDecoderFromMemory( const void *image, size_t image_size );
	CComPtr<IWICBitmapDecoder> CreateDecoderFromFilename( PCTSTR uri );
	CComPtr<IWICFormatConverter> CreateFormatConverter( CComPtr<IWICBitmapDecoder> decoder );
	buffer_t Load( CComPtr<IWICBitmapDecoder> decoder );
};

