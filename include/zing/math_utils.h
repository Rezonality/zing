#pragma once
#include <sstream>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

namespace Zing {

template <class T>
struct NRect
{
    using vec2 = glm::vec<2, T, glm::defaultp>;
    using vec4 = glm::vec<4, T, glm::defaultp>;

    NRect(const vec2& topLeft, const vec2& bottomRight)
        : topLeftPx(topLeft)
        , bottomRightPx(bottomRight)
    {
    }

    NRect(T left, T top, T width, T height)
        : topLeftPx(vec2(left, top))
        , bottomRightPx(vec2(left, top) + vec2(width, height))
    {
    }

    NRect()
    {
    }

    vec2 topLeftPx = vec2(0.0f);
    vec2 bottomRightPx = vec2(0.0f);

    bool Contains(const vec2& pt) const
    {
        return topLeftPx.x <= pt.x && topLeftPx.y <= pt.y && bottomRightPx.x > pt.x && bottomRightPx.y > pt.y;
    }

    vec2 BottomLeft() const
    {
        return vec2(topLeftPx.x, bottomRightPx.y);
    }
    vec2 TopRight() const
    {
        return vec2(bottomRightPx.x, topLeftPx.y);
    }
    vec2 TopLeft() const
    {
        return topLeftPx;
    }

    T Left() const
    {
        return topLeftPx.x;
    }
    T Right() const
    {
        return TopRight().x;
    }
    T Top() const
    {
        return TopRight().y;
    }
    void SetTop(T val)
    {
        topLeftPx.y = val; 
    }
    void SetBottom(T val)
    {
        bottomRightPx.y = val; 
    }
    void SetRight(T val)
    {
        bottomRightPx.x = val; 
    }
    void SetLeft(T val)
    {
        topLeftPx.x = val; 
    }
    T Bottom() const
    {
        return bottomRightPx.y;
    }
    T Height() const
    {
        return bottomRightPx.y - topLeftPx.y;
    }
    T Width() const
    {
        return bottomRightPx.x - topLeftPx.x;
    }
    vec2 Center() const
    {
        return (bottomRightPx + topLeftPx) * .5f;
    }
    vec2 Size() const
    {
        return bottomRightPx - topLeftPx;
    }
    float ShortSide() const
    {
        return std::min(Width(), Height());
    }
    float LongSide() const
    {
        return std::max(Width(), Height());
    }
    float Ratio() const
    {
        return Width() / Height();
    }
    bool Empty() const
    {
        return (Height() == 0.0f || Width() == 0.0f) ? true : false;
    }

    void Clear()
    {
        topLeftPx = vec2();
        bottomRightPx = vec2();
    }

    void Normalize()
    {
        if (topLeftPx.x > bottomRightPx.x)
        {
            std::swap(topLeftPx.x, bottomRightPx.x);
        }
        if (topLeftPx.y > bottomRightPx.y)
        {
            std::swap(topLeftPx.y, bottomRightPx.y);
        }
    }

    void SetSize(const vec2& size)
    {
        bottomRightPx = topLeftPx + size;
    }
    
    void SetWidth(float w)
    {
        bottomRightPx.x = topLeftPx.x + w;
    }
    
    void SetHeight(float h)
    {
        bottomRightPx.y = topLeftPx.y + h;
    }

    void SetSize(T x, T y)
    {
        SetSize(glm::vec2(x, y));
    }

    void Validate()
    {
        if (Width() < 0)
        {
            SetWidth(2.0f);
        }
        if (Height() < 0)
        {
            SetHeight(2.0f);
        }
    }
    NRect<T>& Adjust(T x, T y, T z, T w)
    {
        topLeftPx.x += x;
        topLeftPx.y += y;
        bottomRightPx.x += z;
        bottomRightPx.y += w;
        return *this;
    }

    NRect<T>& Adjust(T x, T y)
    {
        topLeftPx.x += x;
        topLeftPx.y += y;
        bottomRightPx.x += x;
        bottomRightPx.y += y;
        return *this;
    }

    NRect<T>& Adjust(const vec2& v)
    {
        return Adjust(v.x, v.y);
    }

    NRect<T>& Adjust(const vec4& v)
    {
        return Adjust(v.x, v.y, v.z, v.w);
    }

    NRect<T> Adjusted(const vec4& v) const
    {
        NRect<T> adj(topLeftPx, bottomRightPx);
        adj.Adjust(v);
        return adj;
    }

    NRect<T> Adjusted(const vec2& v) const
    {
        NRect<T> adj(topLeftPx, bottomRightPx);
        adj.Adjust(v);
        return adj;
    }

    NRect<T>& Move(T x, T y)
    {
        auto width = Width();
        auto height = Height();
        topLeftPx.x = x;
        topLeftPx.y = y;
        bottomRightPx.x = x + width;
        bottomRightPx.y = y + height;
        return *this;
    }

    NRect<T>& Move(const vec2& v)
    {
        return Move(v.x, v.y);
    }

    NRect<T> Moved(const vec2& v) const
    {
        NRect<T> adj(topLeftPx, bottomRightPx);
        adj.Move(v);
        return adj;
    }

    NRect<T>& Expand(T x, T y, T z, T w)
    {
        topLeftPx.x -= x;
        topLeftPx.y -= y;
        bottomRightPx.x += z;
        bottomRightPx.y += w;
        return *this;
    }

    NRect<T>& Expand(const vec4 v)
    {
        return Expand(v.x, v.y, v.z, v.w);
    }

    NRect<T> Expanded(const vec4 v) const
    {
        NRect<T> adj(topLeftPx, bottomRightPx);
        adj.Expand(v);
        return adj;
    }

    NRect<T>& Clamp(const NRect<T>& r)
    {
        T x, y, z, w;
        x = topLeftPx.x;
        y = topLeftPx.y;
        z = Width();
        w = Height();
        if (x > r.Right())
        {
            x = r.Right();
            z = 0.0;
        }
        else if (x < r.Left())
        {
            z = (x + z) - r.Left();
            x = r.Left();
            z = std::max(0.0f, z);
        }

        if (y > r.Bottom())
        {
            y = r.Bottom();
            w = 0.0;
        }
        else if (y < r.Top())
        {
            w = (y + w) - r.Top();
            y = r.Top();
            w = std::max(0.0f, w);
        }

        if ((x + z) >= r.Right())
        {
            z = r.Right() - x;
            z = std::max(0.0f, z);
        }

        if ((y + w) >= r.Bottom())
        {
            w = r.Bottom() - y;
            w = std::max(0.0f, w);
        }
        topLeftPx = vec2(x, y);
        bottomRightPx = vec2(x + z, y + w);
        return *this;
    }

    bool operator==(const NRect<T>& region) const
    {
        return (topLeftPx == region.topLeftPx) && (bottomRightPx == region.bottomRightPx);
    }
    bool operator!=(const NRect<T>& region) const
    {
        return !(*this == region);
    }
};

template <class T>
inline NRect<T> operator*(const NRect<T>& lhs, float val)
{
    return NRect<T>(lhs.topLeftPx * val, lhs.bottomRightPx * val);
}
template <class T>
inline NRect<T> operator*(const NRect<T>& lhs, const glm::vec2& val)
{
    return NRect<T>(lhs.topLeftPx * val.x, lhs.bottomRightPx * val.y);
}
template <class T>
inline NRect<T> operator-(const NRect<T>& lhs, const NRect<T>& rhs)
{
    return NRect<T>(lhs.topLeftPx.x - rhs.topLeftPx.x, lhs.bottomRightPx.y - rhs.topLeftPx.y);
}
template <class T>
inline NRect<T> operator-(const NRect<T>& lhs, const glm::vec2& pos)
{
    return lhs.Adjusted(-pos);
}
template <class T>
inline NRect<T> operator+(const NRect<T>& lhs, const glm::vec2& pos)
{
    return lhs.Adjusted(pos);
}
template <class T>
inline std::ostream& operator<<(std::ostream& str, const NRect<T>& region)
{
    str << glm::to_string(region.topLeftPx) << ", " << glm::to_string(region.bottomRightPx) << ", size: " << region.Width() << ", " << region.Height();
    return str;
}

enum FitCriteria
{
    X,
    Y
};

template <class T>
inline bool NRectFits(const NRect<T>& area, const NRect<T>& rect, FitCriteria criteria)
{
    if (criteria == FitCriteria::X)
    {
        auto xDiff = rect.bottomRightPx.x - area.bottomRightPx.x;
        if (xDiff > 0)
        {
            return false;
        }
        xDiff = rect.topLeftPx.x - area.topLeftPx.x;
        if (xDiff < 0)
        {
            return false;
        }
    }
    else
    {
        auto yDiff = rect.bottomRightPx.y - area.bottomRightPx.y;
        if (yDiff > 0)
        {
            return false;
        }
        yDiff = rect.topLeftPx.y - area.topLeftPx.y;
        if (yDiff < 0)
        {
            return false;
        }
    }
    return true;
}

using NRectf = NRect<float>;
using NRecti = NRect<long>;

inline float degToRad(float deg)
{
    return deg / 180.0f * 3.1415926f;
}

inline float radToDeg(float rad)
{
    return rad / 3.1415926f * 180.0f;
}

inline glm::vec4 ColorForBackground(const glm::vec4& color)
{
    float luminance = color.r * 0.299f + color.g * 0.587f + color.b * 0.114f;
    if (luminance > 0.5f)
    {
        return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    else
    {
        return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

inline glm::vec4 ModifyAlpha(const glm::vec4& val, float alpha)
{
    return glm::vec4(val.x, val.y, val.z, alpha);
}

} // Zing
