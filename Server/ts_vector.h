#pragma once
#include <vector>
#include <mutex>
#include <functional>


namespace ts_container {
	template <typename T>
	class tsvector {
	private:
		std::vector<T> List;
		std::mutex Lock;
	public:
		void push_back(T Item) {
			std::lock_guard<std::mutex> LocalGuard(this->Lock);
			List.push_back(Item);
		}
		void remove(std::function<bool(T)> Condition) {
			std::lock_guard<std::mutex> LocalGuard(this->Lock);
			for (auto It = this->List.begin(); It != this->List.end(); It++) {
				if (Condition(*It)) {
					this->List.erase(*It);
				}
			}
		}
		T operator[](unsigned int Index) {
			std::lock_guard<std::mutex> LocalGuard(this->Lock);
			return this->List[Index];
		}
};
}