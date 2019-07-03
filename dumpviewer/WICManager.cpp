#include "WICManager.h"
#include <sstream>

#pragma comment(lib, "Windowscodecs")

bool WICManager::Create()
{ return SUCCEEDED( CoCreateInstance( CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, reinterpret_cast< LPVOID * >( &m_Factory ) ) ); }

void WICManager::Release()
{ m_Factory.Release(); }

WICManager::buffer_t WICManager::Load( const void *image, size_t image_size )
{ return Load( CreateDecoderFromMemory( image, image_size ) ); }

WICManager::buffer_t WICManager::Load( PCWSTR uri )
{ return Load( CreateDecoderFromFilename( uri ) ); }

void WICManager::SaveToPng( const LPCTSTR path, const void *pixel_data, int width, int height )
{
	auto encoder = CComPtr<IWICBitmapEncoder>();
	m_Factory->CreateEncoder( GUID_ContainerFormatPng, NULL, &encoder );

	auto stream = CComPtr<IStream>();
	SHCreateStreamOnFileEx( path, STGM_WRITE | STGM_CREATE, FILE_ATTRIBUTE_NORMAL, TRUE, NULL, &stream );

	encoder->Initialize( stream, WICBitmapEncoderNoCache );

	auto frameEncode = CComPtr<IWICBitmapFrameEncode>();
	auto propBag = CComPtr<IPropertyBag2>();
	encoder->CreateNewFrame( &frameEncode, &propBag );

	// デフォルトの設定でエンコーダを初期化
	frameEncode->Initialize( propBag );

	auto fmtOut = GUID_WICPixelFormat32bppBGRA;
	frameEncode->SetSize( width, height );
	frameEncode->SetPixelFormat( &fmtOut );
	frameEncode->WritePixels( height, width * 4, width * 4 * height, reinterpret_cast< LPBYTE >( const_cast< void * >( pixel_data ) ) );
	frameEncode->Commit();

	encoder->Commit();
}

CComPtr<IWICBitmapDecoder> WICManager::CreateDecoderFromMemory( const void *image, size_t image_size )
{
	auto stream = CComPtr<IWICStream>();
	auto hr = m_Factory->CreateStream( &stream );

	if( SUCCEEDED( hr ) )
		hr = stream->InitializeFromMemory( reinterpret_cast< LPBYTE >( const_cast< void * >( image ) ), static_cast< DWORD >( image_size ));

	auto decoder = CComPtr<IWICBitmapDecoder>();
	if( SUCCEEDED( hr ) )
		hr = m_Factory->CreateDecoderFromStream( stream, NULL, WICDecodeMetadataCacheOnLoad, &decoder );

	if( FAILED( hr ) )
	{
		std::stringstream ss;
		ss << "failed CreateDecoderFromMemory: " << hr << std::endl;
		throw std::runtime_error( ss.str() );
	}

	return decoder;
}

CComPtr<IWICBitmapDecoder> WICManager::CreateDecoderFromFilename( PCTSTR uri )
{
	auto decoder = CComPtr<IWICBitmapDecoder>();
	auto hr = m_Factory->CreateDecoderFromFilename( uri, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder );

	if( FAILED( hr ) )
	{
		std::stringstream ss;
		ss << "failed CreateDecoderFromFilename: " << hr << std::endl;
		throw std::runtime_error( ss.str() );
	}

	return decoder;
}

CComPtr<IWICFormatConverter> WICManager::CreateFormatConverter( CComPtr<IWICBitmapDecoder> decoder )
{
	auto source = CComPtr<IWICBitmapFrameDecode>();
	auto hr = decoder->GetFrame( 0, &source );

	auto converter = CComPtr<IWICFormatConverter>();
	if( SUCCEEDED( hr ) )
		hr = m_Factory->CreateFormatConverter( &converter );

	if( SUCCEEDED( hr ) )
		hr = converter->Initialize( source, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeMedianCut );

	if( FAILED( hr ) )
	{
		std::stringstream ss;
		ss << "failed CreateFormatConverter: " << hr << std::endl;
		throw std::runtime_error( ss.str() );
	}

	return converter;
}

WICManager::buffer_t WICManager::Load( CComPtr<IWICBitmapDecoder> decoder )
{
	auto converter = CreateFormatConverter( decoder );
	auto width = UINT( 0 );
	auto height = UINT( 0 );
	auto hr = converter->GetSize( &width, &height );

	if( SUCCEEDED( hr ) )
	{
		auto stride = ( width * 4 + 3 ) & ~0x3;
		auto buffer = buffer_t( sizeof( BITMAPINFOHEADER ) + stride * height, 0 );
		auto bi = reinterpret_cast< BITMAPINFOHEADER * >( buffer.data() );
		auto data = reinterpret_cast< BYTE * >( buffer.data() + sizeof( BITMAPINFOHEADER ) );
		bi->biSize = sizeof( BITMAPINFOHEADER );
		bi->biWidth = width;
		bi->biHeight = -static_cast< LONG >( height );
		bi->biBitCount = 32;
		bi->biPlanes = 1;
		hr = converter->CopyPixels( NULL, stride, stride * height, data );
		if( SUCCEEDED( hr ) )
			return buffer;
	}

	std::stringstream ss;
	ss << "failed WICManager::Load: " << hr << std::endl;
	throw std::runtime_error( ss.str().c_str() );
}
