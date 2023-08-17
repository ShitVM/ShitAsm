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
ShitVM 바이트 파일 경로를 지정하지 않으면 입력 파일이 저장된 디렉터리에 확장자만 `.sbf`로 바꿔 저장합니다.

## 읽을거리
- [예제](examples)
- [문법](docs/Syntax.md)
- [ShitVM 표준 라이브러리](docs/Standard%20Library.md)