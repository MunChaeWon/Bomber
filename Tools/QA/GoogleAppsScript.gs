const BOMBER_QA = Object.freeze({
  spreadsheetId: '1UGXPM7sho_VNmtClY1Nrd-pBGjv7YkrBoXlWgwz534g',
  spreadsheetTitle: 'Bomber QA 테스트 케이스',
  sheetName: '통합 테스트 케이스',
  firstDataRow: 9,
  idColumn: 2,
  actualResultColumn: 13,
  stateColumn: 14,
  commentColumn: 16,
  allowedStates: ['Pass', 'Fail', 'Blocked', 'N/A'],
  automationPaths: {
    'INT-BOMB-001': 'Bomber.Integration.Bomb.EmptyCellPlacement',
    'INT-BOMB-002': 'Bomber.Integration.Bomb.RejectOccupiedCell'
  }
});

function jsonResponse_(body) {
  return ContentService
    .createTextOutput(JSON.stringify(body))
    .setMimeType(ContentService.MimeType.JSON);
}

function doPost(e) {
  const lock = LockService.getScriptLock();
  try {
    lock.waitLock(30000);
    const body = JSON.parse((e && e.postData && e.postData.contents) || '{}');
    const expectedToken = PropertiesService.getScriptProperties().getProperty('BOMBER_QA_TOKEN');

    if (!expectedToken || body.token !== expectedToken) {
      throw new Error('Unauthorized request.');
    }
    if (body.spreadsheetId !== BOMBER_QA.spreadsheetId ||
        body.spreadsheetTitle !== BOMBER_QA.spreadsheetTitle ||
        body.sheetName !== BOMBER_QA.sheetName) {
      throw new Error('Spreadsheet identity mismatch.');
    }
    if (!Array.isArray(body.results) || body.results.length === 0) {
      throw new Error('No test results were supplied.');
    }

    const spreadsheet = SpreadsheetApp.openById(BOMBER_QA.spreadsheetId);
    if (spreadsheet.getName() !== BOMBER_QA.spreadsheetTitle) {
      throw new Error('The target spreadsheet title has changed.');
    }
    const sheet = spreadsheet.getSheetByName(BOMBER_QA.sheetName);
    if (!sheet) {
      throw new Error('Target sheet was not found.');
    }

    const rowCount = Math.max(sheet.getLastRow() - BOMBER_QA.firstDataRow + 1, 1);
    const idValues = sheet
      .getRange(BOMBER_QA.firstDataRow, BOMBER_QA.idColumn, rowCount, 1)
      .getDisplayValues();
    const rowById = {};
    idValues.forEach((row, index) => {
      if (row[0]) rowById[row[0].trim()] = BOMBER_QA.firstDataRow + index;
    });

    const updates = body.results.map(result => {
      if (!result.testId || !rowById[result.testId]) {
        throw new Error(`Unknown test ID: ${result.testId}`);
      }
      if (result.automationPath !== BOMBER_QA.automationPaths[result.testId]) {
        throw new Error(`Automation path mismatch for ${result.testId}.`);
      }
      if (!BOMBER_QA.allowedStates.includes(result.state)) {
        throw new Error(`Invalid state for ${result.testId}: ${result.state}`);
      }
      if (!result.actualResult || !result.comment) {
        throw new Error(`Incomplete result for ${result.testId}.`);
      }
      return { result, row: rowById[result.testId] };
    });

    updates.forEach(item => {
      sheet.getRange(item.row, BOMBER_QA.actualResultColumn).setValue(item.result.actualResult);
      sheet.getRange(item.row, BOMBER_QA.stateColumn).setValue(item.result.state);
      sheet.getRange(item.row, BOMBER_QA.commentColumn).setValue(item.result.comment);
    });
    SpreadsheetApp.flush();

    updates.forEach(item => {
      const saved = sheet.getRange(item.row, BOMBER_QA.actualResultColumn, 1, 4).getDisplayValues()[0];
      if (saved[0] !== item.result.actualResult ||
          saved[1] !== item.result.state ||
          saved[3] !== item.result.comment) {
        throw new Error(`Read-back verification failed for ${item.result.testId}.`);
      }
    });

    return jsonResponse_({
      ok: true,
      updated: updates.length,
      runId: body.runId,
      testIds: updates.map(item => item.result.testId)
    });
  } catch (error) {
    return jsonResponse_({ ok: false, error: String(error && error.message || error) });
  } finally {
    try { lock.releaseLock(); } catch (ignore) {}
  }
}
