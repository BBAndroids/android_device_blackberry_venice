#define LOG_TAG "sensors_wrapper"
#define LOG_NDEBUG 0

#include <log/log.h>
#include <hardware/sensors.h>
#include <string>
#include <dlfcn.h>
#include <errno.h>

static void *lib_handle = NULL;
static struct sensors_module_t *lib_sensors_module;

struct sensor_t sensor_list[20];

int wrapper_get_sensors_list(struct sensors_module_t* module, struct sensor_t const** list)
{
	const struct sensor_t *lib_list;
	int lib_count = lib_sensors_module->get_sensors_list(module, &lib_list);
	if (lib_count < 0)
		return lib_count;
	
	if (lib_count > sizeof(sensor_list) / sizeof(sensor_list[0]))
	{
		ALOGE("%s: vendor sensors reported too many sensors! (%d)", __func__, lib_count);
		return -EINVAL;
	}
	
	int count = 0;
	for (int i = 0; i < lib_count; i++)
	{
		const struct sensor_t *s = &lib_list[i];
		ALOGI("%s: N: %s V: %s H: %d T: %d S: %s P: %s",
		__func__, s->name, s->vendor, s->handle,
		s->type, s->stringType, s->requiredPermission);	
		
		if (s->type > SENSOR_TYPE_HINGE_ANGLE)
		{
			ALOGI(
				"%s: skipped bougus sensor: N: %s V: %s H: %d T: %d S: %s P: %s",
				__func__, s->name, s->vendor, s->handle,
				s->type, s->stringType, s->requiredPermission
			);
			continue;
		}
		
		memcpy(&sensor_list[count++], s, sizeof(struct sensor_t));
	}
	
	*list = sensor_list;
	
	return count;
}

int wrapper_set_operation_mode(unsigned int mode)
{
	return lib_sensors_module->set_operation_mode(mode);
}

int wrapper_open(const struct hw_module_t* module, const char* id, struct hw_device_t** device)
{
	 if (lib_handle == NULL)
	 {
	 	lib_handle = dlopen("/vendor/lib64/hw/sensors.avengers.so", RTLD_LAZY);
	 	if (lib_handle == NULL)
	 	{
			ALOGW("dlerror(): %s", dlerror());
			return -EINVAL;
	 	}
	 	lib_sensors_module = (sensors_module_t *) dlsym(lib_handle, HAL_MODULE_INFO_SYM_AS_STR);
	 }

 	int ret = lib_sensors_module->common.methods->open(&lib_sensors_module->common, id, device);
 	if (ret < 0)
 	{
 		dlclose(lib_handle);
 		lib_handle = NULL;
 	}
 	
 	return ret;
}

static struct hw_module_methods_t wrapper_module_methods = {
    .open = wrapper_open
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = SENSORS_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = SENSORS_HARDWARE_MODULE_ID,
        .name = "BlackBerry Sensors Wrapper Module",
        .author = "balika011",
        .methods = &wrapper_module_methods,
        .dso = NULL,
        .reserved = {0},
    },
    .get_sensors_list = wrapper_get_sensors_list,
    .set_operation_mode = wrapper_set_operation_mode
};
