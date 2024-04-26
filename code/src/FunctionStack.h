#pragma once

#include <stack>
#include <functional>

namespace Mist
{
	class FunctionStack
	{
	public:
		typedef std::function<void()> FnCallback;

		void Add(FnCallback&& fn)
		{
			m_functions.push(fn);
		}

		void Flush()
		{
			while (!m_functions.empty())
			{
				FnCallback fn = m_functions.top();
				m_functions.pop();
				fn();
			}
		}

	private:
		std::stack<FnCallback> m_functions;
	};
}