# ShitAsm
ShitBC 어셈블러

## 컴파일
```
$ git clone --recurse-submodules https://github.com/ShitVM/ShitAsm.git
$ cd ShitAsm
$ cmake CMakeLists.txt
$ make
```

## 사용법
```
$ cd bin
$ ./ShitAsm <입력: ShitBC 어셈블리 경로> [출력: ShitVM 바이트 파일 경로]
```
ShitVM 바이트 파일 경로를 지정하지 않으면 `ShitAsmOutput.sbf` 파일에 결과가 저장됩니다.

## [문법](https://github.com/ShitVM/ShitAsm/blob/master/docs/Syntax.md)