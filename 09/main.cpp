#include <iostream>
#include <functional>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <span>
#include <unordered_set>

//region file mmapping

std::tuple<int, void*, int> mmap_file(const char* filename) {
    // Open the file
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return {0, nullptr, 0};
    }

    // Get the file size
    struct stat file_stat = {};
    if (fstat(fd, &file_stat) == -1) {
        perror("Error getting file stats");
        close(fd);
        return {0, nullptr, 0};
    }
    auto file_size = file_stat.st_size;

    // Memory-map the file
    void* mapped_region = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped_region == MAP_FAILED) {
        perror("Error mapping file");
        close(fd);
        return {0, nullptr, 0};
    }

    return {fd, mapped_region, file_size};
}

void munmap_file(int fd, void* mapped_region, int file_size) {
    if (munmap(mapped_region, file_size) == -1) {
        perror("Error unmapping memory");
    }
    close(fd);
}

//endregion

//region task 1

class disk_map_info {
    std::span<char> data;

public:
    disk_map_info(void* data_ptr, size_t size): data((char*)data_ptr, size) {}

    int get_file_size(size_t file_id) const {
        if (file_id * 2 >= data.size()) return 0;
        return data[file_id * 2] - '0';
    };

    int get_available_blocks_after_file(size_t file_id) const {
        if (file_id * 2 + 1 >= data.size()) return 0;
        return data[file_id * 2 + 1] - '0';
    };

    size_t get_file_cnt() const {
        return (data.size() + 1) >> 1;
    }
};


unsigned long long move_blocks_and_get_checksum(const disk_map_info& disk_map) {

    unsigned long long checksum = 0;
    size_t idx = 0;
    auto set_file_blocks = [&] (size_t cnt, size_t file_id) {
        if (cnt == 0) return;
        checksum += (idx * cnt + cnt * (cnt-1) / 2) * file_id;
        idx += cnt;
    };

    // iterate through disk map from two ends
    size_t lo_file_id = 0, hi_file_id = disk_map.get_file_cnt();
    int hi_file_size = 0;

    while (lo_file_id <= hi_file_id) {

        if (lo_file_id == hi_file_id) {
            set_file_blocks(hi_file_size, lo_file_id);
            break;
        }

        // set all the file blocks of the file lo_file_id
        auto lo_file_size = disk_map.get_file_size(lo_file_id);
        set_file_blocks(lo_file_size, lo_file_id);

        // get the count of empty blocks that can be populated with blocks from hi_file_id
        auto available_blocks = disk_map.get_available_blocks_after_file(lo_file_id);

        while (hi_file_id > lo_file_id && available_blocks > 0) {

            auto blocks_to_add = std::min(hi_file_size, available_blocks);

            // add as many blocks from hi_file_id as possible
            set_file_blocks(blocks_to_add, hi_file_id);
            available_blocks -= blocks_to_add;
            hi_file_size -= blocks_to_add;

            if (hi_file_size == 0) {
                hi_file_size = disk_map.get_file_size(--hi_file_id);
            }

        }

        lo_file_id++;

    }

    return checksum;
}

//endregion


//region task 2

class disk_file_index {
    const disk_map_info& disk_map;
    size_t last_loaded_file_id;

    std::array<std::deque<size_t>, 10> queues;

public:

    disk_file_index(const disk_map_info& disk_map):
        disk_map(disk_map),
        last_loaded_file_id(disk_map.get_file_cnt()) {
    }

    std::optional<size_t> get_file_of_leq_size(int size, size_t min_file_id) {

        std::optional<int> chosen_queue;

        for (int i = size; i > 0; i--) {
            if (queues[i].empty()) {
                continue;
            }

            if (queues[i].front() <= min_file_id) {
                queues[i].clear();
                continue;
            }

            if (!chosen_queue || queues[*chosen_queue].front() < queues[i].front()) {
                chosen_queue = i;
            }

        }

        if (chosen_queue) {
            auto ans = queues[*chosen_queue].front();
            queues[*chosen_queue].pop_front();
            return ans;
        }

        while (last_loaded_file_id - 1 > min_file_id) {
            last_loaded_file_id--;
            auto file_size = disk_map.get_file_size(last_loaded_file_id);

            if (file_size <= size) {
                return last_loaded_file_id;
            }

            queues[file_size].push_back(last_loaded_file_id);
        }

        return {};
    }

};


unsigned long long move_files_and_get_checksum(const disk_map_info& disk_map) {

    unsigned long long checksum = 0;
    size_t idx = 0;

    auto set_file_blocks = [&] (size_t cnt, size_t file_id) {
        if (cnt == 0) return;
        checksum += (idx * cnt + cnt * (cnt-1) / 2) * file_id;
        idx += cnt;
    };

    auto skip_blocks = [&] (size_t cnt) mutable {
        idx += cnt;
    };

    disk_file_index disk_index(disk_map);
    std::unordered_set<size_t> moved_file_ids;

    for (size_t curr_file_id = 0; curr_file_id < disk_map.get_file_cnt(); curr_file_id++) {

        // set all the file blocks of the file curr_file_id
        auto curr_file_size = disk_map.get_file_size(curr_file_id);

        if (moved_file_ids.contains(curr_file_id)) {
            skip_blocks(curr_file_size);
        } else {
            set_file_blocks(curr_file_size, curr_file_id);
        }

        // get the count of contiguous empty blocks
        auto available_blocks = disk_map.get_available_blocks_after_file(curr_file_id);

        while (available_blocks > 0) {

            auto maybe_file_id = disk_index.get_file_of_leq_size(available_blocks, curr_file_id);
            if (!maybe_file_id) {
                skip_blocks(available_blocks);
                break;
            }

            auto blocks_to_add = disk_map.get_file_size(*maybe_file_id);

            // add blocks of *maybe_file_id
            set_file_blocks(blocks_to_add, *maybe_file_id);
            available_blocks -= blocks_to_add;
            moved_file_ids.insert(*maybe_file_id);
        }

    }

    return checksum;
}

//endregion



int main() {
    const char* filename = "input.txt";

    auto [fd, file, file_size] = mmap_file(filename);
    if (file == nullptr) {
        return -1;
    }

    size_t span_size = file_size;
    auto file_as_char_ptr = (char*) file;

    while (span_size > 0 && !std::isdigit(file_as_char_ptr[span_size-1])) {
        std::cout << (int)((char*)file)[span_size-1] << std::endl;
        span_size--;
    }

    disk_map_info disk_map(file_as_char_ptr, span_size);

    std::cout << move_blocks_and_get_checksum(disk_map) << std::endl;
    std::cout << move_files_and_get_checksum(disk_map) << std::endl;

    munmap_file(fd, file, file_size);
}

