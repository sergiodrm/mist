#include "Noise.h"
#include <random>
#include "Utils/TimeUtils.h"
#include "SDL_stdinc.h"
#include "Core/Logger.h"
#include <vector>
#include "Utils/GenericUtils.h"
#include "Core/Debug.h"
#include <stdlib.h>

namespace Mist
{

	// returns between 0...1
	float Random()
	{
		static std::default_random_engine generator;
		static std::uniform_real_distribution distribution(0.f, 1.f);
		return distribution(generator);
	}

	void RandomSeed(uint64_t seed)
	{
		srand(seed);
	}

	namespace NoiseUtils
	{
		void ComputeNoiseIndex(float point, uint32_t size, uint32_t& index0, uint32_t& index1, float& t)
		{
			uint32_t xi = (uint32_t)point - (point < 0 && point != (uint32_t)point);
			t = point - (float)xi;
			index0 = xi & (size - 1);
			index1 = (xi + 1) & (size - 1);
		}
	}

	ValueNoise1D::ValueNoise1D(uint64_t seed)
	{
		RandomSeed(seed);
		for (uint32_t i = 0; i < Size; ++i)
			m_ruler[i] = Random();
	}

	float ValueNoise1D::Evaluate(float point)
	{
		uint32_t i0;
		uint32_t i1;
		float t;
		NoiseUtils::ComputeNoiseIndex(point, Size, i0, i1, t);
		return math::Lerp(m_ruler[i0], m_ruler[i1], t);
	}

	ValueNoise2D::ValueNoise2D(uint64_t seed)
	{
		RandomSeed(seed);
		for (uint32_t i = 0; i < Size; ++i)
		{
			m_ruler[i] = Random();
			m_table[i] = i;
		}

		// shuffle table
		std::shuffle(std::begin(m_table), std::end(m_table), std::mt19937{ std::random_device{}()});
	}

	float ValueNoise2D::Evaluate(const glm::vec2& point) const
	{
		struct  
		{
			uint32_t i0;
			uint32_t i1;
			float t;
		} rx, ry;
		NoiseUtils::ComputeNoiseIndex(point.x, Size, rx.i0, rx.i1, rx.t);
		NoiseUtils::ComputeNoiseIndex(point.y, Size, ry.i0, ry.i1, ry.t);

		const float r00 = Get(rx.i0, ry.i0);
		const float r01 = Get(rx.i0, ry.i1);
		const float r10 = Get(rx.i1, ry.i0);
		const float r11 = Get(rx.i1, ry.i1);

		const float r0 = Interpolate(r00, r10, rx.t);
		const float r1 = Interpolate(r01, r11, rx.t);
		return Interpolate(r0, r1, ry.t);
	}

	float ValueNoise2D::Interpolate(float a, float b, float t) const
	{
		return Mist::math::Lerp(a, b, t);
	}

	float ValueNoise1D::Interpolate(float a, float b, float t) const
	{
		return Mist::math::Lerp(a, b, t);
	}

	float BlueNoise2D::Sample::ComputeDist(const Sample& s0, const Sample& s1)
	{
		float dx = s1.x - s0.x;
		float dy = s1.y - s0.y;
		return dx * dx + dy * dy;
	}

	void BlueNoise2D::Noise::Setup(uint32_t width, uint32_t height, uint32_t dataStride)
	{
		Invalidate();

		check(dataStride > 1 && width && height);
		m_width = width;
		m_height = height;
		m_dataStride = dataStride;

		uint32_t size = width * height;
		check(!(size & 0x1));
		m_data = (float*)_malloc(size * dataStride * sizeof(float));
		memset(m_data, 0.f, size * dataStride * sizeof(float));
	}

	void BlueNoise2D::Noise::Invalidate()
	{
		if (m_data)
		{
			_free(m_data);
			m_data = nullptr;
		}
		memset(this, 0x0, sizeof(Noise));
	}

	void BlueNoise2D::Noise::Push(const Sample& sample)
	{
		check(!IsFull());
		m_data[m_index*m_dataStride + 0] = sample.x;
		m_data[m_index*m_dataStride + 1] = sample.y;
		++m_index;
	}

	bool BlueNoise2D::Noise::IsFull() const
	{
		uint32_t size = m_width * m_height;
		return m_index == size;
	}

	void BlueNoise2D::Noise::Shuffle()
	{
		// TODO: generate the noise already shuffled
		check(m_dataStride == 4);
		std::vector<glm::vec4> hack;
		hack.resize(m_index);
		memcpy_s(hack.data(), hack.size() * m_dataStride * sizeof(float), m_data, hack.size() * m_dataStride * sizeof(float));
		std::random_device rd;
		std::mt19937 g(rd());
		std::shuffle(hack.begin(), hack.end(), g);
		memcpy_s(m_data, hack.size() * m_dataStride * sizeof(float), hack.data(), hack.size() * m_dataStride * sizeof(float));
	}

	BlueNoise2D::Sample BlueNoise2D::Noise::At(uint32_t x, uint32_t y) const
	{
		uint32_t index = y * m_width + x;
		return At(index);
	}

	BlueNoise2D::Sample BlueNoise2D::Noise::At(uint32_t index) const
	{
		check(index < m_index);
		float* p = &m_data[index * m_dataStride];
		return Sample(p[0], p[1]);
	}

	void BlueNoise2D::Grid::Setup(float radius, uint32_t dimensions)
	{
		Invalidate();

		// cell size for spatial partitions
		m_cellSize = radius / sqrtf((float)dimensions);
		m_size = (uint32_t)ceilf(1.f / m_cellSize);
		m_radius = radius;

		// create data container
		m_grid = (uint32_t*)_malloc(m_size * m_size * sizeof(uint32_t));
		memset(m_grid, InvalidIndex, m_size * m_size * sizeof(uint32_t));
	}

	void BlueNoise2D::Grid::Invalidate()
	{
		if (m_grid)
		{
			_free(m_grid);
			m_grid = nullptr;
		}
		m_cellSize = 0.f;
		m_size = 0;
	}

	bool BlueNoise2D::Grid::PutSample(const Sample& sample, Noise& noise)
	{
		uint32_t x = (uint32_t)floorf(sample.x / (float)m_cellSize);
		uint32_t y = (uint32_t)floorf(sample.y / (float)m_cellSize);
		// check if this cell is already occupied.
		if (At(x, y) == InvalidIndex)
		{
			// check if the sample is far enough from neighbours
			for (int i = -1; i < 2; ++i)
			{
				for (int j = -1; j < 2; ++j)
				{
					if (!i && !j) continue;
					int offsetX = x + i;
					int offsetY = y + j;
					if (!(offsetX >= 0 && offsetX < m_size)) continue;
					if (!(offsetY >= 0 && offsetY < m_size)) continue;

					uint32_t noiseIndex = At(offsetX, offsetY);
					if (noiseIndex != InvalidIndex)
					{
						Sample s = noise.At(noiseIndex);
						if (Sample::ComputeDist(s, sample) < m_radius)
							return false;
					}
				}
			}
			// the sample is valid and far enough from their neighbours.
			noise.Push(sample);
			Set(x, y, noise.GetIndex() - 1);
			return true;
		}
		return false;
	}

	uint32_t BlueNoise2D::Grid::At(uint32_t x, uint32_t y) const
	{
		uint32_t cellIndex = y * m_size + x;
		check(cellIndex < m_size * m_size);
		return m_grid[cellIndex];
	}

	void BlueNoise2D::Grid::Set(uint32_t x, uint32_t y, uint32_t index)
	{
		uint32_t cellIndex = y * m_size + x;
		check(cellIndex < m_size * m_size);
		check(m_grid[cellIndex] == InvalidIndex);
		m_grid[cellIndex] = index;
	}
	
	void BlueNoise2D::ActiveList::Setup(uint32_t size)
	{
		Invalidate();

		m_indices = (uint32_t*)_malloc(sizeof(uint32_t) * size);
		memset(m_indices, InvalidIndex, sizeof(uint32_t) * size);
		m_size = size;
	}

	void BlueNoise2D::ActiveList::Invalidate()
	{
		if (m_indices)
		{
			_free(m_indices);
			m_indices = nullptr;
		}
		m_index = 0;
		m_size = 0;
	}

	void BlueNoise2D::ActiveList::Push(uint32_t value)
	{
		check(m_index < m_size);
		check(m_indices[m_index] == InvalidIndex);
		check(value != InvalidIndex);
		m_indices[m_index++] = value;
	}

	uint32_t BlueNoise2D::ActiveList::Pop()
	{
		check(m_index > 0);
		uint32_t i = m_indices[--m_index];
		check(i != InvalidIndex);
		m_indices[m_index] = InvalidIndex;
		return i;
	}

	BlueNoise2D::~BlueNoise2D()
	{
		m_grid.Invalidate();
		m_list.Invalidate();
		m_noise.Invalidate();
	}

	bool BlueNoise2D::Generate(float radius, uint32_t sampleStep, uint32_t width, uint32_t height, uint32_t dataStride)
	{
		PROFILE_SCOPE_LOGF(Generate, "Blue noise 2D - Poisson disk (%f, %d, %dx%d, %d)", radius, sampleStep, width, height, dataStride);
		m_grid.Setup(radius, 2);
		m_noise.Setup(width, height, dataStride);
		m_list.Setup(width * height);

		// put the first sample in the background grid
		Sample sample = { Random(), Random() };
		check(m_grid.PutSample(sample, m_noise));
		m_list.Push(m_noise.GetIndex() - 1);
		while (!m_list.IsEmpty() && !m_noise.IsFull())
		{
			uint32_t index = m_list.Pop();
			Sample sample = m_noise.At(index);

			// generate k samples around current sample
			for (uint32_t i = 0; i < sampleStep; ++i)
			{
				// generate in a random angle and between r and 2r
				Sample newSample = GenerateSample(sample);

				// if true, the sample is valid so we can push it into valid data and in the activeList
				// discard otherwise
				if (m_grid.PutSample(newSample, m_noise))
				{
					if (m_noise.IsFull())
						break;
					m_list.Push(m_noise.GetIndex()-1);
				}
			}
		}
		if (!m_noise.IsFull())
			logferror("Total samples succeded %d/%d. Failed to fill blue noise for input params: [r: %f; k: %d; w: %d; h: %d]\n", 
				m_noise.GetIndex(), width*height,m_grid.GetRadius(), sampleStep, width, height);

		m_noise.Shuffle();

#define TOMB(_f) ((float)(_f) /1024.f/1024.f)
		uint64_t gridMem = m_grid.GetGridSize() * m_grid.GetGridSize() * sizeof(uint32_t);
		uint64_t noiseMem = width * height * dataStride * sizeof(float);
		uint64_t listMem = width * height * sizeof(uint32_t);
		logfinfo("Memory consumed: grid %f MB, noise %f MB, list %f MB. Total: %f MB\n",
			TOMB(gridMem), TOMB(noiseMem), TOMB(listMem), TOMB(gridMem+noiseMem+listMem));
#undef TOMB
		return m_noise.IsFull();
	}

	BlueNoise2D::Sample BlueNoise2D::GenerateSample(const Sample& source) const
	{
		float theta = Random() * 2.f * M_PI;
		float dist = (Random() + 1.f) * m_grid.GetRadius();
		Sample sample;
		sample.x = ::Mist::math::Clamp(source.x + dist * cosf(theta), 0.f, 1.f);
		sample.y = ::Mist::math::Clamp(source.y + dist * sinf(theta), 0.f, 1.f);
		return sample;
	}
}
