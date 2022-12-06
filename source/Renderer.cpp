//External includes
#include "SDL.h"
#include "SDL_surface.h"

//Project includes
#include "Renderer.h"

#include <iostream>

#include "Math.h"
#include "Matrix.h"
#include "Texture.h"
#include "Utils.h"

//#define RENDER_BB

using namespace dae;

Renderer::Renderer(SDL_Window* pWindow) :
	m_pWindow(pWindow)
{
	//Initialize
	SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

	//Create Buffers
	m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
	m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
	m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

	m_pDepthBufferPixels = new float[m_Width * m_Height];

	//Initialize all values to FLT_MAX
	for (int i{0}; i < (m_Width * m_Height); ++i)
		m_pDepthBufferPixels[i] = FLT_MAX;

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });

	m_AspectRatio = static_cast<float>(m_Width) / m_Height;
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
}

void Renderer::Render()
{
	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	// Define Triangle - Vertices in NDC space
	const std::vector<Vertex> verticesWorld
	{
		{{-3, 3, -2}, colors::White},
		{{0, 3, -2}, colors::White},
		{{3, 3, -2}, colors::White},
		{{-3, 0, -2}, colors::White},
		{{0, 0, -2}, colors::White},
		{{3, 0, -2}, colors::White},
		{{-3, -3, -2}, colors::White},
		{{0, -3, -2}, colors::White},
		{{3, -3, -2}, colors::White}
	};

	std::vector<Vertex> verteciesNDC{};

	std::vector<Vector2> verticesRaster;
	VertexTransformationFunction(verticesWorld, verteciesNDC);

	//NDC to Screen Space
	for (const Vertex& ndcVertex : verteciesNDC)
		verticesRaster.push_back({ (ndcVertex.position.x + 1) / 2.0f * m_Width, (1.0f - ndcVertex.position.y) / 2.0f * m_Height });

	//Set depth values to FLT_MAX
	for (int i{ 0 }; i < (m_Width * m_Height); ++i)
	{
		m_pDepthBufferPixels[i] = FLT_MAX;
		m_pBackBufferPixels[i] = 0;
	}

	assert(verticesWorld.size() % 3 == 0);
	//Check if the number of vertecies is divisible by 3.
	//If not then there is an issue with our triangles

	for (int vertexIndex{0}; vertexIndex < verticesWorld.size(); vertexIndex += 3)
	{
		Vector2 bottomLeft{ verticesRaster[vertexIndex + 2].x, verticesRaster[vertexIndex + 2].y };
		Vector2 topRight{ verticesRaster[vertexIndex + 1].x, verticesRaster[vertexIndex].y };

		Utils::Clamp(bottomLeft.x, 0, m_Width - 1);
		Utils::Clamp(topRight.x, 0, m_Width - 1);

		Utils::Clamp(bottomLeft.y, 0, m_Height - 1);
		Utils::Clamp(topRight.y, 0, m_Height - 1);

		const int BBWidth{ int(abs(bottomLeft.x - topRight.x)) };
		const int BBHeight{ int(abs(bottomLeft.y - topRight.y)) };

		for (int px{int(bottomLeft.x)}; px < BBWidth + int(bottomLeft.x); ++px)
		{
			for (int py{int(topRight.y)}; py < BBHeight + int(topRight.y); ++py)
			{
				Vector2 currentPixel{ static_cast<float>(px), static_cast<float>(py) };
				const int currentPixelIdx{ py * m_Width + px };

				const Vector2 v0 = verticesRaster[vertexIndex + 0];
				const Vector2 v1 = verticesRaster[vertexIndex + 1];
				const Vector2 v2 = verticesRaster[vertexIndex + 2];

				const float area = Vector2::Cross(v1 - v0, v2 - v0) / 2.f;
				const float w0 = Vector2::Cross(v2 - v1, currentPixel - v1) / 2.f / area;
				const float w1 = Vector2::Cross(v0 - v2, currentPixel - v2) / 2.f / area;
				const float w2 = Vector2::Cross(v1 - v0, currentPixel - v0) / 2.f / area;
				if (w0 < 0 || w1 < 0 || w2 < 0) continue;

				ColorRGB finalColor = verteciesNDC[vertexIndex + 0].color * w0 + 
					verteciesNDC[vertexIndex + 1].color * w1 + verteciesNDC[vertexIndex + 2].color * w2;

				//const Vector3 triangleMiddle{ verticesWorld[vertexIndex + 0].position * w0 +
				//	verticesWorld[vertexIndex + 1].position * w1 +
				//	verticesWorld[vertexIndex + 2].position * w2 };
				//  const float triangleDepth{ (triangleMiddle - m_Camera.origin).Magnitude() };

				//Depth Test
				const float triangleDepth{ verticesWorld[vertexIndex + 0].position.z * w0 +
					verticesWorld[vertexIndex + 1].position.z * w1 + verticesWorld[vertexIndex + 2].position.z * w2 };

				if (m_pDepthBufferPixels[currentPixelIdx] < triangleDepth)
					continue;

				//Depth Write
				m_pDepthBufferPixels[currentPixelIdx] = triangleDepth;

				//Update Color in Buffer
				finalColor.MaxToOne();
				m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
					static_cast<uint8_t>(finalColor.r * 255),
					static_cast<uint8_t>(finalColor.g * 255),
					static_cast<uint8_t>(finalColor.b * 255));

			}
		}
	}
	

	//@END
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);
}

void Renderer::VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const
{
	//Pre-Allocate the memory to avoid moving
	vertices_out.reserve(vertices_in.size());

	for (Vertex vertex : vertices_in)
	{
		vertex.position = m_Camera.invViewMatrix.TransformPoint(vertex.position);

		vertex.position.x = (vertex.position.x / (m_AspectRatio * m_Camera.fov)) / vertex.position.z;
		vertex.position.y = (vertex.position.y / m_Camera.fov) / vertex.position.z;

		vertices_out.emplace_back(vertex);
	}
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
