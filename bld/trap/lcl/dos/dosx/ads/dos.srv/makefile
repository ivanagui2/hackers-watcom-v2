#pmake: serv lcl os_dos dsx cpu_386 x86
host_cpu = 386
host_os  = dos

target_cpu = x86
target_os  = ads

srv = ads
name = adshelp
which = SERVER

extra_objs = adsacc.obj adsintr.obj

extra_cpp_flags = -DACAD
extra_c_flags = -I"$(adsdir)"
extra_a_flags = -3s -DACAD
extra_linker = op nocaseexact

!include $(trap_dir)/lcl/dos/dosx/dosx.mif
!include $(trap_dir)/master.mif
