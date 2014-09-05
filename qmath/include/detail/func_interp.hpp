namespace math
{
	namespace detail
	{
		template<class Policy> inline float lerp_check_mu(float t)
		{
			assert(t >= 0 && t <= 1);
			return t;
		}
		template<> inline float lerp_check_mu<safe>(float t)
		{
			return clamp(t, 0.f, 1.f);
		}
	}


	template<class Policy> inline float lerp(float a, float b, float t)
	{
		t = detail::lerp_check_mu<Policy>(t);
		float x = b - a;
		return a + x*t;
	}

	template <typename T, class Policy> angle<T> lerp(angle<T> const& a, angle<T> const& b, float t)
	{
		t = detail::lerp_check_mu<Policy>(t);
		auto start = a.radians;
		auto end = b.radians;
		auto difference = abs(end - start);
		if (difference > angle<T>::pi.radians)
		{
			// We need to add on to one of the values.
			if (end > start)
			{
				// We'll add it on to start...
				start += angle<T>::_2pi.radians;
			}
			else
			{
				// Add it on to end.
				end += angle<T>::_2pi.radians;
			}
		}
		return angle<T>(start + ((end - start) * t));
	}

	template <typename T, class Policy> quat<T> lerp(quat<T> const& a, quat<T> const& b, float t)
	{
		t = detail::lerp_check_mu<Policy>(t);
		auto x(b - a);
		return a + x*t;
	}

	template<class T, class Policy> inline T lerp(T const& a, T const& b, float t)
	{
		t = detail::lerp_check_mu<Policy>(t);
		return (T)(a*(1.f - t) + b*t);
	}

	template<class T, class Policy> inline vec2<T> lerp(vec2<T> const& a, vec2<T> const& b, float t)
	{
		t = detail::lerp_check_mu<Policy>(t);
		auto x(b - a);
		return a + x*t;
	}

	template<class T, class Policy> inline vec3<T> lerp(vec3<T> const& a, vec3<T> const& b, float t)
	{
		t = detail::lerp_check_mu<Policy>(t);
		auto x(b - a);
		return a + x*t;
	}

	template<class T, class Policy> inline vec4<T> lerp(vec4<T> const& a, vec4<T> const& b, float t)
	{
		t = detail::lerp_check_mu<Policy>(t);
		auto x(b - a);
		return a + x*t;
	}

	template<typename T, class Policy> quat<T> nlerp(quat<T> const& a, quat<T> const& b, float t)
	{
		t = detail::lerp_check_mu<Policy>(t);
		quat<T> result(quat<T>::uninitialized);
		T angle = dot(a, b);
		if (angle >= 0)
		{
			result = lerp(a, b, t);
		}
		else if (angle <= -0.9999)
		{
			result = t < T(0.5) ? a : b;
		}
		else
		{
			result = lerp(a, -b, t);
		}
		return normalized(result);
	}

	template<typename T, class Policy> quat<T> slerp(quat<T> const& a, quat<T> const& b, float t)
	{
		t = detail::lerp_check_mu<Policy>(t);

		T angle = dot(a, b);

		T scale;
		T invscale;
		if (angle > T(0.998))
		{
			scale = T(1) - t;
			invscale = t;
			return quat<T>((a*scale) + (b*invscale));
		}
		else 
		{
			if (angle < T(0))
			{
				if (angle <= T(-0.9999))
				{
					return quat<T>(t < T(0.5) ? a : b);
				}
				else
				{
					quat<T> qa(-a);
					angle = T(-1) * max(angle, T(-1));
					T const theta = acos(angle);
					T const invsintheta = T(1) / sin(theta);
					scale = sin(theta * (T(1) - t)) * invsintheta;
					invscale = sin(theta * t) * invsintheta;
					return quat<T>((qa * scale) + (b*invscale));
				}
			}
			else
			{
				T const theta = acos(angle);
				T const invsintheta = T(1) / sin(theta);
				scale = sin(theta * (T(1) - t)) * invsintheta;
				invscale = sin(theta * t) * invsintheta;
				return quat<T>((a*scale) + (b*invscale));
			}
		}
	}
}