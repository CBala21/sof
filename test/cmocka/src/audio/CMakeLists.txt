# SPDX-License-Identifier: BSD-3-Clause

add_subdirectory(buffer)
add_subdirectory(component)
add_subdirectory(pcm_converter)
if(CONFIG_COMP_MIXER)
	add_subdirectory(mixer)
endif()
add_subdirectory(pipeline)
if(CONFIG_COMP_VOLUME)
	add_subdirectory(volume)
endif()
if(CONFIG_COMP_MUX)
	add_subdirectory(mux)
endif()
if(CONFIG_COMP_SEL)
	add_subdirectory(selector)
endif()
if(CONFIG_COMP_IIR)
	add_subdirectory(eq_iir)
endif()
if(CONFIG_COMP_FIR)
	add_subdirectory(eq_fir)
endif()
if(CONFIG_COMP_DRC)
	add_subdirectory(drc)
endif()
