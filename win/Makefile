# Copyright Radu-Stefan Minea 334CA [2022]

build: libscheduler.dll

libscheduler.dll: so_scheduler.obj
	link /nologo /dll /out:libscheduler.dll so_scheduler.obj

so_scheduler.obj: so_scheduler.c
	cl /nologo /W3 /D_CRT_SECURE_NO_DEPRECATE /DDLL_IMPORTS /Foso_scheduler.obj /c so_scheduler.c

clean:
	del /Q /F *.obj *.lib *.dll *.exe  *.exp