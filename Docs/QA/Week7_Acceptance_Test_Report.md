# 7주차 인수 테스트 작업 보고서

## 작업 목적

사용자가 실제 게임을 이용하는 관점에서 게임 시작, 핵심 폭탄 플레이, 승리 결과 안내의 주요 흐름을 검증한다.

## 작성한 테스트 케이스

| ID | 자동화 경로 | 사용자 관점 검증 내용 | 현재 상태 |
|---|---|---|---|
| ACC-GAME-001 | `Bomber.Acceptance.Game.StartPlayableMatch` | 메인 맵 시작 후 플레이 가능한 캐릭터와 폭탄 기능 준비 확인 | Pass · 0.17초 |
| ACC-BOMB-001 | `Bomber.Acceptance.Bomb.ClearObstacle` | 폭탄을 설치하고 폭발시켜 파괴 가능한 장애물을 제거하는 핵심 플레이 확인 | Pass · 2.38초 |
| ACC-RESULT-001 | `Bomber.Acceptance.Round.ShowVictoryResult` | 승리 상태 변경 후 게임 결과 데이터 반영 확인 | Pass · 0.17초 |

## 자동화 구성

- 실행 파일: `RunBomberAcceptanceQA.bat`
- 테스트 필터: `Bomber.Acceptance.`
- 결과 기록: Google Sheet `인수 테스트 케이스`
- 실패 시: Jira 결함 생성 또는 기존 자동화 이슈에 실행 내용 추가
- 실행 완료 시: 성공·실패 수를 포함한 Slack 알림 발송

## 최종 결과

2026-10-28 최종 실행에서 인수 테스트 3개가 모두 성공했다. Google Sheets의 3개 결과 행을 갱신하고 다시 읽어 반영을 확인했으며 Slack 요약 알림도 정상 전송했다.

- 최종 실행 보고서: `Saved/Automation/BAT-Acceptance-20261028-221928`
- 정상 결과: 3 Pass / 0 Fail / 0 Blocked
- Google Sheets: 3개 행 갱신 및 재확인 완료
- Slack: 최종 성공 요약 알림 전송 완료

## 실패 및 수정 기록

첫 실행에서는 헤드리스 PIE 환경에서 로컬 HUD를 필수 준비 조건으로 사용해 3개 테스트가 30초 제한 시간에 실패했다. 이 실행으로 Google Sheets 실패 기록, Jira `MOON-3`~`MOON-5` 생성, Slack 실패 알림까지 정상 동작하는 것을 확인했다.

이후 자동화 환경에서도 실제 게임 기능을 검증할 수 있도록 준비 조건을 생성 맵, 플레이 가능한 캐릭터, 폭탄 기능, 플레이어 상태 기준으로 수정했다. 재빌드와 재실행 후 3개가 모두 통과했다. HUD의 실제 시각 표현은 Unreal Editor 관찰 실행과 영상 촬영에서 별도로 확인한다.

## 남은 작업

1. Unreal Editor에서 동일 테스트를 관찰 실행해 영상 증빙 촬영
2. PPT의 증빙 자리표시자를 촬영 이미지로 교체
