# ShitAsm
ShitBC 어셈블러

## 컴파일
```
$ git clone https://github.com/ShitVM/ShitAsm.git --recurse-submodules
$ cd ShitAsm
$ cmake .
$ make
```

## 사용법
```
$ cd bin
$ ./ShitAsm <입력: ShitBC 어셈블리 경로> [출력: ShitVM 바이트 파일 경로]
```
ShitVM 바이트 파일 경로를 지정하지 않으면 입력 파일과 같은 이름/경로에 확장자만 sbf로 바꿔 저장합니다.

## [문법](https://github.com/ShitVM/ShitAsm/blob/master/docs/Syntax.md)
## [예제](https://github.com/ShitVM/ShitAsm/tree/master/examples)