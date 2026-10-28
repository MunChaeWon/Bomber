# 5주차 통합 테스트 결과 보고서

## 1. 테스트 개요

- 대상 프로젝트: Bomberrage
- QA 집중 범위: 폭탄 설치 조건
- 테스트 단계: 통합 테스트
- 테스트 프레임워크: Unreal Automation Framework
- 실행 환경: Unreal Engine 5.7, Visual Studio 2022, Windows
- 작업 브랜치: `qa/week5-integration-tests`
- 최종 자동 실행일: 2026-10-10

이번 주에는 실제 게임 맵을 PIE로 실행한 상태에서 플레이어, 셀 점유 정보, 폭탄 액터가 함께 동작하는 폭탄 설치 통합 테스트를 구성했다. 테스트는 에디터의 Automation 창과 명령줄 비관찰 모드에서 동일하게 실행할 수 있다.

## 2. 구현한 테스트

| 테스트 ID | 자동화 테스트 경로 | 검증 내용 | 결과 |
| --- | --- | --- | --- |
| `INT-BOMB-001` | `Bomber.Integration.Bomb.EmptyCellPlacement` | 빈 셀에서 폭탄 설치 입력 후 해당 셀에 폭탄 액터가 1개 생성되는지 확인 | 성공 |
| `INT-BOMB-002` | `Bomber.Integration.Bomb.RejectOccupiedCell` | 벽 또는 상자가 점유한 셀에 폭탄 설치 입력을 보내도 폭탄 수가 0으로 유지되는지 확인 | 성공 |

두 테스트 모두 `/Game/Bomber/Maps/Main`에서 PIE를 시작하고 실제 게임 객체 사이의 상호작용을 검증한다. 육안 확인과 촬영을 위해 판정 후 PIE 화면을 5초 동안 유지하도록 구성했다.

## 3. 최종 자동 실행 결과

`RunBomberQA.bat`을 사용해 에디터가 종료된 상태에서 `Bomber.Integration.Bomb` 그룹을 비관찰 모드로 실행했다.

| 테스트 ID | 실행 시간 | 오류 | 경고 | 최종 상태 |
| --- | ---: | ---: | ---: | --- |
| `INT-BOMB-001` | 5.36초 | 0 | 0 | Pass |
| `INT-BOMB-002` | 6.17초 | 0 | 0 | Pass |

최종 결과: **2개 성공 / 경고 0개 / 실패 0개**

- 자동 실행 시각: 2026-10-10 19:19:34 KST
- 자동화 보고서: `Saved/Automation/BAT-20261010-191934`
- 보고서 형식: Unreal Automation HTML 및 JSON

`Saved` 폴더의 보고서와 로그는 로컬 실행 산출물이므로 Git에 포함하지 않는다.

## 4. BAT 자동 실행 및 결과 검증

프로젝트 루트의 `RunBomberQA.bat`은 다음 순서로 동작한다.

1. 실행 중인 Unreal Editor가 있는지 확인한다.
2. Unreal Engine 설치 경로를 프로젝트의 `EngineAssociation`과 Windows 레지스트리에서 찾는다.
3. `UnrealEditor-Cmd.exe`로 폭탄 통합 테스트를 실행한다.
4. 실행별 새 폴더에 생성된 `index.json`을 읽는다.
5. 테스트 ID와 전체 자동화 경로가 예상값과 일치하는지 확인한다.
6. 결과를 `Pass`, `Fail`, `Blocked`로 변환한다.
7. 검증된 결과만 Google Sheet에 전송한다.
8. 시트에 저장된 값을 다시 읽어 반영 여부를 확인한다.

이전 실행 보고서가 새 결과로 기록되지 않도록 실행마다 고유한 보고서 폴더를 사용하고 파일 생성 시각을 검사한다. 시트 전송에 실패하더라도 검증된 페이로드는 `Saved/Automation/SheetSync`에 남는다.

## 5. Google Sheet 연동

결과 기록 대상은 제목이 정확히 `Bomber QA 테스트 케이스`인 문서의 `통합 테스트 케이스` 탭이다.

- `Actual Result`: 자동화 검증 내용, 실행 시간, 오류·경고 수
- `State`: `Pass`, `Fail`, `Blocked` 중 하나
- `Comment`: 실행 시각

Apps Script는 문서 ID, 문서 제목, 탭 이름, 테스트 ID, 자동화 경로를 모두 검증한 뒤 기존 행만 갱신한다. 로컬 웹 앱 URL과 인증 토큰은 `Tools/QA/qa-sheet.local.json`에 저장하며 이 파일은 Git에서 제외한다.

최종 연동 시험에서 2개 테스트 행이 `Pass`로 기록됐고, 시트 요약 수식은 `Pass 2 / Fail 0 / Blocked 0`으로 계산됐다.

## 6. 증빙 자료 촬영 항목

사진과 영상은 실제 에디터 화면을 사용해 별도로 촬영한다.

- [ ] `INT-BOMB-001`: 빈 셀에 폭탄이 생성된 PIE 화면
- [ ] `INT-BOMB-002`: 벽 또는 상자 점유 셀에 폭탄이 생성되지 않은 PIE 화면
- [ ] Automation 창에서 두 테스트가 초록색으로 완료된 화면
- [ ] BAT 실행 완료 화면
- [ ] Google Sheet에서 두 테스트가 `Pass`로 기록된 화면
- [ ] 필요 시 001·002 동작을 연속으로 확인할 수 있는 영상

촬영한 이미지는 `Docs/QA/Evidence`에 추가하고, 제출 전 이 절에 이미지 링크를 연결한다. 영상은 저장소 용량을 고려해 제출 플랫폼 또는 외부 공유 위치를 사용한다.

## 7. 결론 및 다음 단계

5주차에는 빈 셀의 정상 폭탄 설치와 점유 셀의 설치 거부를 실제 게임 객체 수준에서 검증했다. 또한 동일 테스트를 BAT로 반복 실행하고 결과를 Google Sheet에 자동 기록하는 흐름까지 완성했다.

다음 통합 테스트 후보는 다음과 같다.

1. 화력 수치에 따른 폭발 범위
2. 벽과 상자의 폭발 반응
3. 폭발 피해에 따른 플레이어 체력 감소
4. 연쇄 폭발
