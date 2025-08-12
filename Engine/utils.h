#pragma once

#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>

using namespace std;

#define LOG(...) cout << __VA_ARGS__ << endl;

#define VK_CHECK(x)                                                                \
    do                                                                             \
    {                                                                              \
        VkResult err = x;                                                          \
        if (err != VK_SUCCESS)                                                     \
        {                                                                          \
            std::cerr << "Vulkan error (" << err << ") at " << __FILE__            \
                      << ":" << __LINE__ << " -> " << #x << std::endl;             \
            std::abort();                                                          \
        }                                                                          \
    } while (0)

/*
* Finds property count and populates corresponding data vector
*/
template <typename T, typename F, typename... Args>
vector<T> VK_ENUMERATE(F function, Args&&... args)
{
	uint32_t count = 0;
	function(forward<Args>(args)..., &count, nullptr);

	if (count > 0)
	{
		vector<T> result(count);
		function(forward<Args>(args)..., &count, result.data());
		return result;
	}
	else
	{
		vector<T> result(0);
		return result;
	}
}

/*
* Checks if all members of required vector are present in available vector
*/
template <typename T>
bool VK_VALIDATE(const vector<const char*>& required, const vector<T>& available, const char* (*get_name)(const T&))
{
	for (auto item : required)
	{
		bool found = false;
		for (auto& available_item : available)
		{
			if (strcmp(get_name(available_item), item) == 0)
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			return false;
		}
	}
	return true;
}

inline const char* getExtensionName(const VkExtensionProperties& extension) { return extension.extensionName; }

inline const char* getLayerName(const VkLayerProperties& layer) { return layer.layerName; }