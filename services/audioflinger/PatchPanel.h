/*
**
** Copyright 2014, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#pragma once

namespace android {

class PatchPanel : public IAfPatchPanel {
public:
    explicit PatchPanel(AudioFlinger* audioFlinger) : mAudioFlinger(*audioFlinger) {}

    /* List connected audio ports and their attributes */
    status_t listAudioPorts(unsigned int *num_ports,
        struct audio_port* ports) final;

    /* Get supported attributes for a given audio port */
    status_t getAudioPort(struct audio_port_v7* port) final;

    /* Create a patch between several source and sink ports */
    status_t createAudioPatch(const struct audio_patch *patch,
                              audio_patch_handle_t *handle,
                              bool endpointPatch = false) final;

    /* Release a patch */
    status_t releaseAudioPatch(audio_patch_handle_t handle) final;

    /* List connected audio devices and they attributes */
    status_t listAudioPatches(unsigned int *num_patches,
            struct audio_patch* patches) final;

    // Retrieves all currently estrablished software patches for a stream
    // opened on an intermediate module.
    status_t getDownstreamSoftwarePatches(audio_io_handle_t stream,
            std::vector<SoftwarePatch>* patches) const final;

    // Notifies patch panel about all opened and closed streams.
    void notifyStreamOpened(AudioHwDevice *audioHwDevice, audio_io_handle_t stream,
                            struct audio_patch* patch) final;
    void notifyStreamClosed(audio_io_handle_t stream) final;

    void dump(int fd) const final;

    // Call with AudioFlinger mLock held
    const std::map<audio_patch_handle_t, Patch>& patches_l() const final { return mPatches; }

    // Must be called under AudioFlinger::mLock
    status_t getLatencyMs_l(audio_patch_handle_t patchHandle, double* latencyMs) const final;

    void closeThreadInternal_l(const sp<IAfThreadBase>& thread) const final;

private:
    AudioHwDevice* findAudioHwDeviceByModule(audio_module_handle_t module);
    sp<DeviceHalInterface> findHwDeviceByModule(audio_module_handle_t module);
    void addSoftwarePatchToInsertedModules(
            audio_module_handle_t module, audio_patch_handle_t handle,
            const struct audio_patch *patch);
    void removeSoftwarePatchFromInsertedModules(audio_patch_handle_t handle);
    /**
     * erase the patch referred by its handle.
     * @param handle of the patch to be erased
     * @param reuseExistingHalPatch if set, do not trig the callback of listeners, listener
     * would receive instead a onUpdateAudioPatch when the patch will be recreated.
     * It prevents for example DeviceEffectManager to spuriously remove / add a device on an already
     * opened input / output mix.
     */
    void erasePatch(audio_patch_handle_t handle, bool reuseExistingHalPatch = false);

    /**
     * Returns true if the old and new patches passed as arguments describe the same
     * connections between the first sink and the first source
     * @param oldPatch previous patch
     * @param newPatch new patch
     * @return true if the route is unchanged between the old and new patch, false otherwise
     */
    inline bool patchesHaveSameRoute(
            const struct audio_patch &newPatch, const struct audio_patch &oldPatch) const {
        return (newPatch.sources[0].type == AUDIO_PORT_TYPE_DEVICE &&
                oldPatch.sources[0].type == AUDIO_PORT_TYPE_DEVICE &&
                newPatch.sources[0].id == oldPatch.sources[0].id &&
                newPatch.sinks[0].type == AUDIO_PORT_TYPE_MIX &&
                oldPatch.sinks[0].type == AUDIO_PORT_TYPE_MIX &&
                newPatch.sinks[0].ext.mix.handle == oldPatch.sinks[0].ext.mix.handle) ||
                (newPatch.sinks[0].type == AUDIO_PORT_TYPE_DEVICE &&
                oldPatch.sinks[0].type == AUDIO_PORT_TYPE_DEVICE &&
                newPatch.sinks[0].id == oldPatch.sinks[0].id &&
                newPatch.sources[0].type == AUDIO_PORT_TYPE_MIX &&
                oldPatch.sources[0].type == AUDIO_PORT_TYPE_MIX &&
                newPatch.sources[0].ext.mix.handle == oldPatch.sources[0].ext.mix.handle);
    }

    AudioFlinger &mAudioFlinger;
    std::map<audio_patch_handle_t, Patch> mPatches;

    // This map allows going from a thread to "downstream" software patches
    // when a processing module inserted in between. Example:
    //
    //  from map value.streams                               map key
    //  [Mixer thread] --> [Virtual output device] --> [Processing module] ---\
    //       [Harware module] <-- [Physical output device] <-- [S/W Patch] <--/
    //                                                 from map value.sw_patches
    //
    // This allows the mixer thread to look up the threads of the software patch
    // for propagating timing info, parameters, etc.
    //
    // The current assumptions are:
    //   1) The processing module acts as a mixer with several outputs which
    //      represent differently downmixed and / or encoded versions of the same
    //      mixed stream. There is no 1:1 correspondence between the input streams
    //      and the software patches, but rather a N:N correspondence between
    //      a group of streams and a group of patches.
    //   2) There are only a couple of inserted processing modules in the system,
    //      so when looking for a stream or patch handle we can iterate over
    //      all modules.
    struct ModuleConnections {
        std::set<audio_io_handle_t> streams;
        std::set<audio_patch_handle_t> sw_patches;
    };
    std::map<audio_module_handle_t, ModuleConnections> mInsertedModules;
};

}  // namespace android
