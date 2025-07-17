#
# Copyright (c) 2023-2024 NVIDIA CORPORATION AND AFFILIATES.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted
# provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright notice, this list of
#       conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright notice, this list of
#       conditions and the following disclaimer in the documentation and/or other materials
#       provided with the distribution.
#     * Neither the name of the NVIDIA CORPORATION nor the names of its contributors may be used
#       to endorse or promote products derived from this software without specific prior written
#       permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NVIDIA CORPORATION BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TOR (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

project('DOCA_SAMPLE', 'C', 'CPP',
	# Get version number from file.
	version: run_command(find_program('cat'),
		files('/opt/mellanox/doca/applications/VERSION'), check: true).stdout().strip(),
	license: 'BSD-3',
	# default_options: ['buildtype=debug'],
	    default_options: [
        'buildtype=debug',  # 调试模式（默认包含-g）
        'debug=true',       # 强制生成调试符号
        'optimization=0'    # 禁用优化（避免代码被编译器重排，调试更直观）
    ],
	meson_version: '>= 0.61.2'
)

SAMPLE_NAME = 'rdma_write_requester'

# Comment this line to restore warnings of experimental DOCA features
add_project_arguments('-D DOCA_ALLOW_EXPERIMENTAL_API', language: ['c', 'cpp'])

sample_dependencies = []
# Required for all DOCA programs
sample_dependencies += dependency('doca-common')
# The DOCA library of the sample itself
sample_dependencies += dependency('doca-rdma')
# Utility DOCA library for executables
sample_dependencies += dependency('doca-argp')

sample_srcs = [
	# The sample itself
	SAMPLE_NAME + '_sample.c',
	# Main function for the sample's executable
	SAMPLE_NAME + '_my.c',
	# Common code for the DOCA library samples
	'../rdma_common_pool_th.c',
	# Common code for all DOCA samples
	'../../common.c',
	# '../mmap_pool.c',
	'../rdma_control_enum.c'  # ✅ 要加这个
	,'../doca_hp_shm.c'
]

sample_inc_dirs  = []
# Common DOCA library logic
sample_inc_dirs += include_directories('..')
# Common DOCA logic (samples)
sample_inc_dirs += include_directories('../..')
# Common DOCA logic
sample_inc_dirs += include_directories('/opt/mellanox/doca')
# Common DOCA logic (applications)
sample_inc_dirs += include_directories('/opt/mellanox/doca/applications/common/')

executable('_doca_' + SAMPLE_NAME, sample_srcs,
	#c_args : '-Wno-missing-braces',
	    c_args : [
        '-Wno-missing-braces',
        '-g',  # 显式添加调试符号（debug模式已默认包含，但冗余添加更保险）
        '-O0'  # 显式禁用优化（与optimization=0对应）
    ],
	dependencies : sample_dependencies,
	include_directories: sample_inc_dirs,
	install: false)
