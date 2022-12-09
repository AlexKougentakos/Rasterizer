#include "Texture.h"
#include "Vector2.h"
#include <SDL_image.h>

namespace dae
{
	Texture::Texture(SDL_Surface* pSurface) :
		m_pSurface{ pSurface },
		m_pSurfacePixels{ (uint32_t*)pSurface->pixels }
	{
	}

	Texture::~Texture()
	{
		if (m_pSurface)
		{
			SDL_FreeSurface(m_pSurface);
			m_pSurface = nullptr;
		}
	}

	Texture* Texture::LoadFromFile(const std::string& path)
	{
		Texture* texture = new Texture{ IMG_Load(path.c_str()) };
		return texture;
	}

	ColorRGB Texture::Sample(const Vector2& uv) const
	{
		unsigned char r{}, g{}, b{};

		const int x{ int(uv.x * m_pSurface->w) };
		const int y{ int(uv.y * m_pSurface->h) };

		unsigned const int pixel{ m_pSurfacePixels[x + y * m_pSurface->w] };

		SDL_GetRGB(pixel, m_pSurface->format, &r, &g, &b);

		constexpr float inverseClampedValue{ 1 / 255.f };

		return { r * inverseClampedValue, g * inverseClampedValue, b * inverseClampedValue };
	}
}