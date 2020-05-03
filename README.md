# ShitAsm
ShitBC 어셈블러

## 브랜치
- master: 주 개발 브랜치
- stable: 안정된 릴리즈 채널
- unstable: 불안정한 릴리즈 채널
- hotfix: 릴리즈 된 커밋에 존재하는 버그를 수정하는 개발 브랜치

**주의**: 릴리즈 채널에는 커밋을 하거나 PR을 넣지 마세요!

## 컴파일
```
$ git clone https://github.com/ShitVM/ShitAsm.git --recurse-submodules
$ cd ShitAsm
$ cmake --config --build .
```

## 사용법
```
$ cd bin
$ ./ShitAsm <입력: ShitBC 어셈블리 경로> [출력: ShitVM 바이트 파일 경로]
```
ShitVM 바이트 파일 경로를 지정하지 않으면 입력 파일과 같은 이름/경로에 확장자만 sbf로 바꿔 저장합니다.

## [문법](https://github.com/ShitVM/ShitAsm/blob/master/docs/Syntax.md)
## [예제](https://github.com/ShitVM/ShitAsm/tree/master/examples)