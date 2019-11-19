#pragma once
#include "simpleguidx11.h"
#include "structs.h"
#include "time.h"
#include <FreeImage.h>
#include <fstream>

class SimpleGuiDX11
{
public:	
	SimpleGuiDX11( const int width, const int height );	
	~SimpleGuiDX11();		
	
	int MainLoop();	

protected:
	int Init();
	int Cleanup();	

	void CreateRenderTarget();
	void CleanupRenderTarget();
	HRESULT CreateDeviceD3D( HWND hWnd );
	void CleanupDeviceD3D();

	HRESULT CreateTexture();
	LRESULT WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
	static LRESULT CALLBACK s_WndProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );		

	virtual int Ui();
	virtual Color4f get_pixel( const int x, const int y, const float t = 0.0f );

	void sample(int x, int y, float t, Color4f * result);

	void Producer();

	int width() const;
	int height() const;
	int current() const;
	float progress() const;

	bool vsync_{ true };
	std::chrono::duration<float> lastFrame_;
	std::chrono::duration<float> running_;

private:	
	WNDCLASSEX wc_;
	HWND hwnd_;

	ID3D11Device * g_pd3dDevice{ nullptr };
	ID3D11DeviceContext * g_pd3dDeviceContext{ nullptr };
	IDXGISwapChain * g_pSwapChain{ nullptr };
	ID3D11RenderTargetView * g_mainRenderTargetView{ nullptr };

	ID3D11Texture2D * tex_id_{ nullptr };
	ID3D11ShaderResourceView * tex_view_{nullptr};
	int width_{ 640 };
	int height_{ 480 };
	int current_{ 0 };
	float * tex_data_{ nullptr }; // DXGI_FORMAT_R32G32B32A32_FLOAT
	std::mutex tex_data_lock_;
		
	std::atomic<bool> finish_request_{ false };	
};
