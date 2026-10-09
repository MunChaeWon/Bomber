# Bomberrage QA Automation

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7-0E1128?logo=unrealengine)
![Platform](https://img.shields.io/badge/Platform-Windows-0078D6?logo=windows)
![Language](https://img.shields.io/badge/Language-C%2B%2B-00599C?logo=cplusplus)
![Tests](https://img.shields.io/badge/Unit%20Tests-6%20Passing-brightgreen)

## 프로젝트 소개

이 저장소는 오픈 소스 Unreal Engine 게임 **Bomberrage**를 대상으로 테스트 자동화 환경을 구축하는 QA 프로젝트입니다.

Unreal Automation Framework를 이용해 게임 코드의 단위 테스트부터 통합·시스템·인수 테스트까지 단계적으로 확장하고, 코드 변경 시 기존 기능이 깨지지 않았는지 반복 검증하는 회귀 테스트 체계를 만드는 것을 목표로 합니다.

> 이 저장소의 게임 코드와 콘텐츠는 [JanSeliv/Bomber](https://github.com/JanSeliv/Bomber)를 기반으로 합니다. 이 Fork에서 새롭게 진행하는 작업 범위는 QA 자동화 설계와 테스트 코드 작성입니다.

## QA 목표

- 핵심 C++ 로직을 작은 단위로 검증
- 기능 사이의 연결과 게임 플레이 흐름을 단계적으로 검증
- 동일한 테스트를 반복 실행할 수 있는 회귀 테스트 구성
- 테스트 결과를 보고서로 남겨 실패 원인을 추적
- 향후 GitHub 기반 자동 실행 환경으로 확장

## 개발 및 테스트 환경

| 구분 | 사용 환경 |
| --- | --- |
| 게임 엔진 | Unreal Engine 5.7 |
| 개발 도구 | Visual Studio 2022 |
| 언어 | C++ |
| 테스트 프레임워크 | Unreal Automation Framework |
| 운영체제 | Windows |
| 현재 에셋 | Blockout Map 기반 최소 구성 |

현재 단위 테스트와 기본 통합 테스트에는 Blockout Map 구성이 충분합니다. 전체 그래픽 리소스는 추후 시각적 검증과 완성된 플레이 흐름을 다루는 시스템 테스트 단계에서 적용할 예정입니다.

## 현재 진행 상태

현재는 **4주차 단위 테스트 단계**입니다.

| 테스트 경로 | 검증 내용 | 결과 |
| --- | --- | --- |
| `Bomber.Unit.Smoke` | 테스트 모듈 로드 및 실행 여부 | 통과 |
| `Bomber.Unit.Cell.Construction` | 셀 좌표 생성 시 정수 단위 반올림 | 통과 |
| `Bomber.Unit.Cell.Validity` | 정상 셀과 `InvalidCell` 판별 | 통과 |
| `Bomber.Unit.Cell.Equality` | 동일·상이한 좌표 및 `InvalidCell` 비교 | 통과 |
| `Bomber.Unit.Cell.Arithmetic` | 덧셈·뺄셈·복합 대입·배율 연산 | 통과 |
| `Bomber.Unit.Cell.Direction` | 방향 enum과 셀의 양방향 변환 | 통과 |

최근 전체 실행 결과: **6개 성공 / 경고 0 / 실패 0**

## 테스트 코드 구조

```text
Source/BomberTests/
├─ BomberTests.Build.cs
└─ Private/
   ├─ BomberTestsModule.cpp
   └─ Unit/
      ├─ BomberSmokeTest.cpp
      ├─ BmrCellConstructionTest.cpp
      ├─ BmrCellValidityTest.cpp
      ├─ BmrCellEqualityTest.cpp
      ├─ BmrCellArithmeticTest.cpp
      └─ BmrCellDirectionTest.cpp
```

`BomberTests`는 에디터 전용 테스트 모듈입니다. 게임 실행용 코드와 테스트 코드를 분리해 실제 게임 빌드에 테스트 코드가 포함되지 않도록 구성했습니다.

## 테스트 실행 방법

### Unreal Editor에서 실행

1. 프로젝트를 Unreal Editor로 실행합니다.
2. `Tools > Test Automation`을 엽니다.
3. `Bomber.Unit` 항목을 선택합니다.
4. 테스트를 실행하고 성공·실패 결과를 확인합니다.

### 명령줄에서 전체 단위 테스트 실행

아래의 `<UE_ROOT>`와 `<PROJECT_ROOT>`를 각 PC의 실제 경로로 변경합니다.

```powershell
& "<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "<PROJECT_ROOT>\Bomber.uproject" `
  -unattended -nop4 -nosplash -NullRHI `
  '-ExecCmds=Automation RunTest Bomber.Unit;Quit' `
  '-TestExit=Automation Test Queue Empty' `
  '-ReportExportPath=<PROJECT_ROOT>\Saved\Automation\Unit' `
  -log
```

실행 결과는 지정한 `Saved/Automation` 경로에 HTML과 JSON 보고서로 생성됩니다. `Saved` 폴더는 로컬 실행 결과이므로 Git에 커밋하지 않습니다.

## 주차별 진행 계획

| 주차 | 목표 | 상태 |
| --- | --- | --- |
| 1주차 | 프로젝트 선정과 QA 자동화 목표 정의 | 완료 |
| 2주차 | `Source`, `Plugins`, `Content` 구조 분석 | 완료 |
| 3주차 | Unreal·Visual Studio 빌드 환경 구성 | 완료 |
| 4주차 | 핵심 C++ 로직 단위 테스트 작성 | 진행 중 |
| 5주차 | 주요 객체와 기능 간 통합 테스트 | 예정 |
| 이후 | 시스템·인수 테스트 및 GitHub 자동 실행 | 예정 |

## 테스트 확장 계획

다음 단위 테스트 후보는 아래와 같습니다.

- 빈 셀 집합의 기본 반환값
- 경계값과 잘못된 입력 처리

단위 테스트가 안정화되면 맵 생성, 폭탄 설치·폭발, 캐릭터 피해 처리처럼 여러 객체가 함께 동작하는 통합 테스트로 확장합니다.

## 브랜치 운영

- `master`: 개인 Fork의 기준 브랜치
- `qa/week4-unit-tests`: 4주차 단위 테스트 작업 브랜치
- `upstream`: 원본 `JanSeliv/Bomber` 저장소
- `origin`: QA 작업을 저장하는 `MunChaeWon/Bomber` Fork

테스트 변경은 작업 브랜치에서 검증한 뒤 Pull Request를 통해 기준 브랜치에 반영하는 방식을 사용합니다.

## 원본 프로젝트 및 라이선스

- 원본 프로젝트: [JanSeliv/Bomber](https://github.com/JanSeliv/Bomber)
- 원본 게임: Bomberrage
- 원본 개발자: Yevhenii Selivanov 및 기여자
- 라이선스: [MIT License](LICENSE)

게임 코드와 기존 콘텐츠의 저작권은 원본 프로젝트의 작성자와 기여자에게 있습니다. 이 저장소의 QA 관련 변경 사항은 테스트 자동화 학습과 프로젝트 수행을 목적으로 합니다.
