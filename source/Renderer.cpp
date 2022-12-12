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

	m_AspectRatio = float(m_Width) / float(m_Height);

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,5.f,-30.f }, m_AspectRatio);

	m_pTexture = Texture::LoadFromFile("Resources/tuktuk.png");

	Utils::ParseOBJ("Resources/tuktuk.obj", m_Mesh.vertices, m_Mesh.indices);
	m_Mesh.primitiveTopology = PrimitiveTopology::TriangleList;
}

Renderer::~Renderer()
{

	delete[] m_pDepthBufferPixels;
	delete m_pTexture;
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

	// Define Triangles - Vertices in NDC space
	std::vector<Mesh> meshesWorldSpace{};
	meshesWorldSpace.push_back(m_Mesh);
	//{
	//	Mesh
	//	{
	//		{
	//			Vertex{{-3,3,-2},colors::White,{0,0}},
	//			Vertex{{0,3,-2},colors::White,{.5,0}},
	//			Vertex{{3,3,-2},colors::White,{1,0}},

	//			Vertex{{-3,0,-2},colors::White,{0,0.5}},
	//			Vertex{{0,0,-2},colors::White,{.5,.5}},
	//			Vertex{{3,0,-2},colors::White,{1,.5}},

	//			Vertex{{-3,-3,-2},colors::White,{0,1}},
	//			Vertex{{0,-3,-2},colors::White,{.5,1}},
	//			Vertex{{3,-3,-2},colors::White,{1,1}}
	//		},

	//	  {
	//	  	3,0,4,1,5,2,
	//	  	2,6,
	//	  	6,3,7,4,8,5
	//	  },
	//	  PrimitiveTopology::TriangleStrip
	//	//{
	//	//	3,0,1,	1,4,3,	4,1,2,
	//	//	2,5,4,	6,3,4,	4,7,6,
	//	//	7,4,5,	5,8,7
	//	//},
	//	//PrimitiveTopology::TriangleList
	//	}
	//};
	
	for (Mesh& mesh : meshesWorldSpace)
	{
		VertexTransformationFunction(mesh);

		std::vector<Vector2> verteciesRaster;
		for (const Vertex_Out& ndcVertex : mesh.vertices_out)
			verteciesRaster.push_back({ (ndcVertex.position.x + 1) / 2.0f * m_Width, 
				(1.0f - ndcVertex.position.y) / 2.0f * m_Height });

		//Clear depth buffer & background
		for (int i{ 0 }; i < (m_Width * m_Height); ++i)
		{
			m_pDepthBufferPixels[i] = FLT_MAX;
			m_pBackBufferPixels[i] = 0;
		}

		assert(mesh.vertices_out.size() % 3 == 0);
		//Check if the number of vertecies is divisible by 3.
		//If not then there is an issue with our triangles

		if (mesh.primitiveTopology == PrimitiveTopology::TriangleList)
			for (int vertexIndex{0}; vertexIndex < mesh.indices.size(); vertexIndex += 3)
				RenderTriangle(mesh, verteciesRaster, vertexIndex, false);
		if (mesh.primitiveTopology == PrimitiveTopology::TriangleStrip)
			for (int startVertexIndex{ 0 }; startVertexIndex < mesh.indices.size() - 2; ++startVertexIndex)
				RenderTriangle(mesh, verteciesRaster, startVertexIndex, startVertexIndex % 2);
	}

	//@END
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);
}

void Renderer::RenderTriangle(const Mesh& mesh, const std::vector<Vector2>& verteciesRaster,
	int vertexIndex, bool swapVertex) const
{
	const size_t vertexIndex0{ mesh.indices[vertexIndex + (2 * swapVertex)] };
	const size_t vertexIndex1{ mesh.indices[vertexIndex + 1] };
	const size_t vertexIndex2{ mesh.indices[vertexIndex + (!swapVertex * 2)] };

	// Make sure the triangle doesn't have the same vertex twice. If it does it's got no area so we don't have to render it.
	if (vertexIndex0 == vertexIndex1 || vertexIndex1 == vertexIndex2 || vertexIndex2 == vertexIndex0)
		return;

	const Vector2 vertex0{ verteciesRaster[vertexIndex0] };
	const Vector2 vertex1{ verteciesRaster[vertexIndex1] };
	const Vector2 vertex2{ verteciesRaster[vertexIndex2] };

	const Vector2 vertex0NDC = { mesh.vertices_out[vertexIndex0].position.x, mesh.vertices_out[vertexIndex0].position.y };
	const Vector2 vertex1NDC = { mesh.vertices_out[vertexIndex1].position.x, mesh.vertices_out[vertexIndex1].position.y };
	const Vector2 vertex2NDC = { mesh.vertices_out[vertexIndex2].position.x, mesh.vertices_out[vertexIndex2].position.y };
	
	if (vertex0NDC.x < -1.f || vertex0NDC.x > 1.f ||
		vertex0NDC.y < -1.f || vertex0NDC.y > 1.f ||
		vertex1NDC.x < -1.f || vertex1NDC.x > 1.f ||
		vertex1NDC.y < -1.f || vertex1NDC.y > 1.f ||
		vertex2NDC.x < -1.f || vertex2NDC.x > 1.f ||
		vertex2NDC.y < -1.f || vertex2NDC.y > 1.f) return;

	// Define the Bounding Box
	Vector2 bottomLeft{ Vector2::SmallestVectorComponents(vertex0,Vector2::SmallestVectorComponents(vertex1,vertex2)) };
	Vector2 topRight{ Vector2::BiggestVectorComponents(vertex0,Vector2::BiggestVectorComponents(vertex1,vertex2)) };

	// Add the margin to fix the black lines between the triangles
	constexpr float margin{ 1.f };
	bottomLeft -= {margin, margin};
	topRight += {margin, margin};

	Utils::Clamp(bottomLeft.x, 0, float(m_Width) - 1);
	Utils::Clamp(topRight.x, 0, float(m_Width) - 1);
	Utils::Clamp(bottomLeft.y, 0, float(m_Height) - 1);
	Utils::Clamp(topRight.y, 0, float(m_Height) - 1);
	
	for (int px{ int(bottomLeft.x) }; px < int(topRight.x); ++px)
	{
		for (int py{ int(bottomLeft.y)}; py < int(topRight.y); ++py)
		{
			const Vector2 currentPixel{ static_cast<float>(px),static_cast<float>(py) };
			const int pixelIdx{ px + py * m_Width };

			if (Utils::IsInTriangle(currentPixel, vertex0, vertex1, vertex2))
			{
				ColorRGB finalColor{};
				float weight0 = Vector2::Cross(currentPixel - vertex1, vertex1 - vertex2);
				float weight1 = Vector2::Cross(currentPixel - vertex2, vertex2 - vertex0);
				float weight2 = Vector2::Cross(currentPixel - vertex0, vertex0 - vertex1);

				const float totalTriangleArea{ Vector2::Cross(vertex1 - vertex0,vertex2 - vertex0) };
				const float invTotalTriangleArea{ 1 / totalTriangleArea };
				weight0 *= invTotalTriangleArea;
				weight1 *= invTotalTriangleArea;
				weight2 *= invTotalTriangleArea;

				const float depth0{ (mesh.vertices_out[vertexIndex0].position.z) };
				const float depth1{ (mesh.vertices_out[vertexIndex1].position.z) };
				const float depth2{ (mesh.vertices_out[vertexIndex2].position.z) };
				const float interpolatedDepth{ 1.f / 
						(weight0 * (1.f / depth0) + 
						weight1 * (1.f / depth1) + 
						weight2 * (1.f / depth2)) };

				if (m_pDepthBufferPixels[pixelIdx] < interpolatedDepth ||
					interpolatedDepth < 0.f || interpolatedDepth > 1.f) continue;

				m_pDepthBufferPixels[pixelIdx] = interpolatedDepth;

				const float wDepth0{ mesh.vertices_out[vertexIndex0].position.w };
				const float wDepth1{ mesh.vertices_out[vertexIndex1].position.w };
				const float wDepth2{ mesh.vertices_out[vertexIndex2].position.w };

				const float wInterpolated{ 1.f /
					(weight0 * (1.f / wDepth0) +
					weight1 * (1.f / wDepth1) +
					weight2 * (1.f / wDepth2)) };

				const Vector2 vertex0UV{ mesh.vertices_out[vertexIndex0].uv /
										mesh.vertices_out[vertexIndex0].position.w };
				const Vector2 vertex1UV{ mesh.vertices_out[vertexIndex1].uv /
										mesh.vertices_out[vertexIndex1].position.w };
				const Vector2 vertex2UV{ mesh.vertices_out[vertexIndex2].uv /
										mesh.vertices_out[vertexIndex2].position.w };

				const Vector2 UVInterpolated{ (vertex0UV * weight0 +
					vertex1UV * weight1 +
					vertex2UV * weight2) * wInterpolated };

				auto remap = [](float value, float min, float max)
				{
					return (value - min) / (max - min);
				};
				
				const float remappedResult = remap(interpolatedDepth, 0.985f, 1.f);

				switch (m_CurrentRenderingMode)
				{
				case RenderingModes::texture:
					finalColor = m_pTexture->Sample(UVInterpolated);
					break;
					//todo fix bounding box rendering
				case RenderingModes::boundingBox:
					finalColor = colors::White;
					break;
				case RenderingModes::depthValues:
					finalColor = { remappedResult, remappedResult,remappedResult };
					break;
				}

				//Update Color in Buffer
				finalColor.MaxToOne();

				m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
					static_cast<uint8_t>(finalColor.r * 255),
					static_cast<uint8_t>(finalColor.g * 255),
					static_cast<uint8_t>(finalColor.b * 255));
			}
		}
	}
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

void Renderer::VertexTransformationFunction(Mesh& mesh) const
{
	const Matrix worldViewMatrix = mesh.worldMatrix * m_Camera.viewMatrix * m_Camera.projectionMatrix;

	for (const Vertex& vertex : mesh.vertices)
	{
		Vertex_Out vertexOut{Vector4{}, vertex.color, vertex.uv};

		vertexOut.position = worldViewMatrix.TransformPoint({ vertex.position, 1.f });

		vertexOut.position.x /= vertexOut.position.w;
		vertexOut.position.y /= vertexOut.position.w;
		vertexOut.position.z /= vertexOut.position.w;

		mesh.vertices_out.push_back(vertexOut);
	}
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}

void Renderer::ToggleRenderMode()
{
	switch (m_CurrentRenderingMode)
	{
	case RenderingModes::texture:
		m_CurrentRenderingMode = RenderingModes::depthValues;
		break;
	case RenderingModes::depthValues:
		m_CurrentRenderingMode = RenderingModes::boundingBox;
		break;
	case RenderingModes::boundingBox:
		m_CurrentRenderingMode = RenderingModes::texture;
		break;
	}
}