/* headers/SortUtils.h */
#ifndef SORT_UTILS_H
#define SORT_UTILS_H

#include <vector>
#include <utility>

namespace sort_utils {

template<typename T, typename KeyFunc>
void insertionSort(std::vector<T>& vec, KeyFunc key) {
    for (size_t i = 1; i < vec.size(); i++) {
        T key_elem = std::move(vec[i]);
        auto key_val = key(key_elem);
        int j = static_cast<int>(i) - 1;
        while (j >= 0 && key(vec[j]) > key_val) {
            vec[j + 1] = std::move(vec[j]);
            j--;
        }
        vec[j + 1] = std::move(key_elem);
    }
}

template<typename T>
void insertionSort(std::vector<T>& vec) {
    for (size_t i = 1; i < vec.size(); i++) {
        T key_elem = std::move(vec[i]);
        int j = static_cast<int>(i) - 1;
        while (j >= 0 && vec[j] > key_elem) {
            vec[j + 1] = std::move(vec[j]);
            j--;
        }
        vec[j + 1] = std::move(key_elem);
    }
}

} // namespace sort_utils

#endif
