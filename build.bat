set src=..\src\

if not exist build_oxitest mkdir build_oxitest
if not exist build_oxidbg mkdir build_oxidbg

pushd build_oxitest
cl /nologo %src%oxitest.c
popd

pushd build_oxidbg
cl /nologo %src%oxidbg.c
popd