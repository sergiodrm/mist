#pragma once

#include "Core/Types.h"
#include <glm/glm.hpp>

namespace Mist
{

    // Returns values between 0 and 1
    float Random();
    void RandomSeed(uint64_t seed = 111);

    class ValueNoise1D
    {
    public:
        ValueNoise1D(uint64_t seed = 113);

        float Evaluate(float point);
    private:
        float Interpolate(float a, float b, float t) const;
        static constexpr uint32_t Size = 256;
        float m_ruler[Size];
    };

    class ValueNoise2D
    {
    public:
        ValueNoise2D(uint64_t seed = 113);

        float Evaluate(const glm::vec2& point) const;
    private:
        float Interpolate(float a, float b, float t) const;
        inline float Get(uint32_t x, uint32_t y) const 
        { 
            check(x < Size && y < Size);
            return m_ruler[m_table[(m_table[x] + y)%Size]];
        }

        static constexpr uint32_t Size = 256;
        float m_ruler[Size];
        uint32_t m_table[Size];
    };

    // Returns 2D blue noise with 4 channels
    class BlueNoise2D
    {
        enum : uint32_t { InvalidIndex = UINT32_MAX };

        struct Sample
        {
            float x;
            float y;

            static float ComputeDist(const Sample& s0, const Sample& s1);
        };

        class Noise
        {
        public:
            void Setup(uint32_t width, uint32_t height, uint32_t dataStride = 4);
            void Invalidate();

            void Push(const Sample& sample);
            uint32_t GetIndex() const { return m_index; }
            bool IsFull() const;
            float* GetNoise() const { return m_data; }

            void Shuffle();

            Sample At(uint32_t x, uint32_t y) const;
            Sample At(uint32_t index) const;

        private:
            float* m_data = nullptr;
            uint32_t m_dataStride = 0;
            uint32_t m_width = 0;
            uint32_t m_height = 0;
            uint32_t m_index = 0;
        };

        class Grid
        {
        public:
            void Setup(float radius, uint32_t dimensions = 2);
            void Invalidate();

            bool PutSample(const Sample& sample, Noise& noise);
            uint32_t At(uint32_t x, uint32_t y) const;
            float GetRadius() const { return m_radius; }
            uint32_t GetGridSize() const { return m_size; }

        private:
            void Set(uint32_t x, uint32_t y, uint32_t index);

        private:
            uint32_t* m_grid = nullptr;
            float m_cellSize = 0.f;
            uint32_t m_size = 0;
            float m_radius = 0.f;
        };

        class ActiveList
        {
        public:
            void Setup(uint32_t size);
            void Invalidate();
            void Push(uint32_t value);
            uint32_t Pop();
            inline bool IsEmpty() const { return !m_index; }
        private:
            uint32_t* m_indices = nullptr;
            uint32_t m_index = 0;
            uint32_t m_size = 0;
        };

    public:
        ~BlueNoise2D();
        bool Generate(float radius, uint32_t sampleStep, uint32_t width, uint32_t height, uint32_t dataStride);
        float* GetNoise() const { return m_noise.GetNoise(); }
    protected:
        Sample GenerateSample(const Sample& source) const;

    private:
        Grid m_grid;
        Noise m_noise;
        ActiveList m_list;
    };
}