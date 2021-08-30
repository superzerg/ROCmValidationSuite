/********************************************************************************
 *
 * Copyright (c) 2018 ROCm Developer Tools
 *
 * MIT LICENSE:
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/

#include "hip/hip_runtime.h"
#include "hip/hip_runtime_api.h"

#include <string>
#include <vector>
#include <iostream>
#include <regex>
#include <utility>
#include <algorithm>
#include <map>

#include "include/rvs_key_def.h"
#include "include/rvs_util.h"
#include "include/rvsactionbase.h"
#include "include/rvsloglp.h"
#include "include/action.h"
#include "include/rvs_memworker.h"
#include "include/gpu_util.h"

using std::string;
using std::vector;
using std::map;
using std::regex;

std::string rvs_mem[]={
    "Test 1  [Walking 1 bit]",
    "Test 2  [Own address test]",
    "Test 3  [Moving inversions, ones&zeros]",
    "Test 4  [Moving inversions, 8 bit pat]",
    "Test 5  [Moving inversions, random pattern]",
    "Test 6  [Block move, 64 moves]",
    "Test 7  [Moving inversions, 32 bit pat]",
    "Test 8  [Random number sequence]",
    "Test 9  [Modulo 20, random pattern]",
    "Test 10 [Bit fade test]",
    "Test 11 [Memory stress test]",
};


/**
 * @brief default class constructor
 */
mem_action::mem_action() {
    bjson = false;
}

/**
 * @brief class destructor
 */
mem_action::~mem_action() {
    property.clear();
}

/**
 * @brief runs the MEM test stress session
 * @param mem_gpus_device_index <gpu_index, gpu_id> map
 * @return true if no error occured, false otherwise
 */
bool mem_action::do_mem_stress_test(map<int, uint16_t> mem_gpus_device_index) {
    size_t k = 0;
    string    msg;

    for (;;) {
        unsigned int i = 0;
        if (property_wait != 0)  // delay mem execution
            sleep(property_wait);

        vector<MemWorker> workers(mem_gpus_device_index.size());

        map<int, uint16_t>::iterator it;

        // all worker instances have the same json settings
        MemWorker::set_use_json(bjson);

        msg = "[" + action_name + "] " + MODULE_NAME + " " +
            " " + " The following memory tests will run";
        rvs::lp::Log(msg, rvs::logresults);

        for (int i = 0; i < 11; i++) {
            msg = "=============== " + rvs_mem[i] + "\n"; 
            rvs::lp::Log(msg, rvs::logresults);
        }

        msg = "[" + action_name + "] " + MODULE_NAME + " " +
            " " + " Starting all workers"; 
        rvs::lp::Log(msg, rvs::logtrace);

        for (it = mem_gpus_device_index.begin();
                it != mem_gpus_device_index.end(); ++it) {

            // set worker thread stress test params
            workers[i].set_name(action_name);
            workers[i].set_gpu_id(it->second);
            workers[i].set_gpu_device_index(it->first);
            workers[i].set_run_wait_ms(property_wait);
            workers[i].set_run_duration_ms(property_duration);
            workers[i].set_mapped_mem(useMappedMemory);
            workers[i].set_num_mem_blocks(max_num_blocks);
            workers[i].set_threads_per_block(threadsPerBlock);
            workers[i].set_pattern(pattern);
            workers[i].set_num_passes(num_passes);
            workers[i].set_stress(stress);
            workers[i].set_num_iterations(num_iterations);

            i++;
        }

        if (property_parallel) {
            for (i = 0; i < mem_gpus_device_index.size(); i++)
                workers[i].start();

            // join threads
            for (i = 0; i < mem_gpus_device_index.size(); i++)
                workers[i].join();
        } else {
            for (i = 0; i < mem_gpus_device_index.size(); i++) {
                workers[i].start();
                workers[i].join();

                // check if stop signal was received
                if (rvs::lp::Stopping())
                    return false;
            }
        }

        // check if stop signal was received
        if (rvs::lp::Stopping())
            return false;

        if (property_count != 0) {
            k++;
            if (k == property_count)
                break;
        }
    }

    return rvs::lp::Stopping() ? false : true;
}

/**
 * @brief reads all MEM-related configuration keys from
 * the module's properties collection
 * @return true if no fatal error occured, false otherwise
 */
bool mem_action::get_all_mem_config_keys(void) {
    string    ststress;
    bool      bsts;
    string    msg;

    bsts = true;

    msg = "[" + action_name + "] " + MODULE_NAME + " " +
            " " + " Getting all mem properties"; 
    rvs::lp::Log(msg, rvs::logtrace);

    if (property_get_int<uint64_t>(RVS_CONF_NUM_BLOCKS,
                     &max_num_blocks, MEM_DEFAULT_NUM_BLOCKS)) {
        msg = "invalid '" +
        std::string(RVS_CONF_NUM_BLOCKS) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    if (property_get_int<uint64_t>(RVS_CONF_NUM_PASSES,
                     &num_passes, MEM_DEFAULT_NUM_PASSES)) {
        msg = "invalid '" +
        std::string(RVS_CONF_NUM_PASSES) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    if (property_get_int<uint64_t>(RVS_CONF_THRDS_PER_BLK,
                     &threadsPerBlock, MEM_DEFAULT_THRDS_BLK)) {
        msg = "invalid '" +
        std::string(RVS_CONF_THRDS_PER_BLK) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    if (property_get<bool>(RVS_CONF_MEM_STRESS,
                     &stress, MEM_DEFAULT_STRESS)) {
        msg = "invalid '" +
        std::string(RVS_CONF_MEM_STRESS) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    if (property_get<bool>(RVS_CONF_MAPPED_MEM,
                     &useMappedMemory, MEM_DEFAULT_MAPPED_MEM)) {
        msg = "invalid '" +
        std::string(RVS_CONF_MAPPED_MEM) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    if (property_get_int<uint64_t>(RVS_CONF_NUM_ITER,
                     &num_iterations, MEM_DEFAULT_NUM_ITERATIONS)) {
        msg = "invalid '" +
        std::string(RVS_CONF_NUM_ITER) + "' key value";
        rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
        bsts = false;
    }

    return bsts;
}

/**
 * @brief reads all common configuration keys from
 * the module's properties collection
 * @return true if no fatal error occured, false otherwise
 */
bool mem_action::get_all_common_config_keys(void) {
    string msg, sdevid, sdev;
    int error;
    bool bsts = true;

    msg = "[" + action_name + "] " + MODULE_NAME + " " +
            " " + " Getting all common properties"; 
    rvs::lp::Log(msg, rvs::logtrace);

    // get <device> property value (a list of gpu id)
    if (int sts = property_get_device()) {
      switch (sts) {
      case 1:
        msg = "Invalid 'device' key value.";
        break;
      case 2:
        msg = "Missing 'device' key.";
        break;
      }
      rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
      bsts = false;
    }

    // get the <deviceid> property value if provided
    if (property_get_int<uint16_t>(RVS_CONF_DEVICEID_KEY,
                                  &property_device_id, 0u)) {
      msg = "Invalid 'deviceid' key value.";
      rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
      bsts = false;
    }

    // get the other action/MEM related properties
    if (property_get(RVS_CONF_PARALLEL_KEY, &property_parallel, false)) {
      msg = "invalid '" +
          std::string(RVS_CONF_PARALLEL_KEY) + "' key value";
      rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
      bsts = false;
    }

    error = property_get_int<uint64_t>
    (RVS_CONF_COUNT_KEY, &property_count, DEFAULT_COUNT);
    if (error != 0) {
      msg = "invalid '" +
          std::string(RVS_CONF_COUNT_KEY) + "' key value";
      rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
      bsts = false;
    }

    error = property_get_int<uint64_t>
    (RVS_CONF_WAIT_KEY, &property_wait, DEFAULT_WAIT);
    if (error != 0) {
      msg = "invalid '" +
          std::string(RVS_CONF_WAIT_KEY) + "' key value";
      bsts = false;
    }

    return bsts;
}

/**
 * @brief gets the number of ROCm compatible AMD GPUs
 * @return run number of GPUs
 */
int mem_action::get_num_amd_gpu_devices(void) {
    int hip_num_gpu_devices;
    string msg;

    hipGetDeviceCount(&hip_num_gpu_devices);
    if (hip_num_gpu_devices == 0) {  // no AMD compatible GPU
        msg = action_name + " " + MODULE_NAME + " " + MEM_NO_COMPATIBLE_GPUS;
        rvs::lp::Log(msg, rvs::logerror);

        if (bjson) {
            unsigned int sec;
            unsigned int usec;
            rvs::lp::get_ticks(&sec, &usec);
            void *json_root_node = rvs::lp::LogRecordCreate(MODULE_NAME,
                            action_name.c_str(), rvs::loginfo, sec, usec);
            if (!json_root_node) {
                // log the error
                string msg = std::string(JSON_CREATE_NODE_ERROR);
                rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
                return -1;
            }

            rvs::lp::AddString(json_root_node, "ERROR", MEM_NO_COMPATIBLE_GPUS);
            rvs::lp::LogRecordFlush(json_root_node);
        }
        return 0;
    }
    return hip_num_gpu_devices;
}

bool mem_action::get_gpu_list(int hip_devices, map<int, uint16_t> &mem_gpus_device_index){
    bool amd_gpus_found = false;
    for (int i = 0; i < hip_devices; i++) {
        // get GPU device properties
        hipDeviceProp_t props;
        hipGetDeviceProperties(&props, i);

        // compute device location_id (needed in order to identify this device
        // in the gpus_id/gpus_device_id list
        unsigned int dev_location_id =
            ((((unsigned int) (props.pciBusID)) << 8) | (((unsigned int) (props.pciDeviceID)) << 3));

        uint16_t devId;
        if (rvs::gpulist::location2device(dev_location_id, &devId)) {
          continue;
        }

        // filter by device id if needed
        if (property_device_id > 0 && property_device_id != devId)
          continue;

        // check if this GPU is part of the GPU stress test
        // (device = "all" or the gpu_id is in the device: <gpu id> list)
        bool cur_gpu_selected = false;
        uint16_t gpu_id;
        // if not and AMD GPU just continue
        if (rvs::gpulist::location2gpu(dev_location_id, &gpu_id))
          continue;
        if (property_device_all) {
            cur_gpu_selected = true;
        } else {
            // search for this gpu in the list
            // provided under the <device> property
            auto it_gpu_id = find(property_device.begin(),
                                  property_device.end(),
                                  gpu_id);

            if (it_gpu_id != property_device.end())
                cur_gpu_selected = true;
        }

        if (cur_gpu_selected) {
            mem_gpus_device_index.insert
                (std::pair<int, uint16_t>(i, gpu_id));
            amd_gpus_found = true;
        }
    }
    return amd_gpus_found;

}
/**
 * @brief gets all selected GPUs and starts the worker threads
 * @return run result
 */
int mem_action::get_all_selected_gpus(void) {
    int hip_num_gpu_devices;
    bool amd_gpus_found = false;
    map<int, uint16_t> mem_gpus_device_index;
    std::string msg;

    hip_num_gpu_devices = get_num_amd_gpu_devices();
    if (hip_num_gpu_devices < 1)
        return hip_num_gpu_devices;

    msg = "[" + action_name + "] " + MODULE_NAME + " " +
            " " + "Scan for GPU IDs"; 
    rvs::lp::Log(msg, rvs::logtrace);

    amd_gpus_found = get_gpu_list(hip_num_gpu_devices, mem_gpus_device_index)
    if (amd_gpus_found) {
        if (do_mem_stress_test(mem_gpus_device_index))
            return 0;
        else 
            return -1;
    } else {
      msg = "No devices match criteria from the test configuation.";
      rvs::lp::Err(msg, MODULE_NAME_CAPS, action_name);
      return -1;
    }

    msg = "[" + action_name + "] " + MODULE_NAME + " " +
            " " + "Got all the GPU IDs"; 
    rvs::lp::Log(msg, rvs::logtrace);

    return 0;
}

/**
 * @brief runs the whole MEM logic
 * @return run result
 */
int mem_action::run(void) {
    string msg;

    msg = "[" + action_name + "] " + MODULE_NAME + " " +
            " " + "Getting properties of memory test"; 
    rvs::lp::Log(msg, rvs::logtrace);

    // get the action name
    if (property_get(RVS_CONF_NAME_KEY, &action_name)) {
      rvs::lp::Err("Action name missing", MODULE_NAME_CAPS);
      return -1;
    }

    // check for -j flag (json logging)
    if (property.find("cli.-j") != property.end())
        bjson = true;

    if (!get_all_common_config_keys())
        return -1;
    if (!get_all_mem_config_keys())
        return -1;


    return get_all_selected_gpus();
}



