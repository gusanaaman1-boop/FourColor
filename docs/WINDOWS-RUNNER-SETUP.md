# בניית ווינדוס אוטומטית — התקנה חד־פעמית

**המטרה:** בכל פעם שאני דוחפת קוד, מחשב הווינדוס שלך בונה את הפלאגין ומעלה
אותו ל־GitHub כקובץ מוכן להורדה. אתה לא צריך לעשות כלום מעבר להורדה.

**עלות: אפס.** לא צריך כרטיס אשראי והרפו נשאר פרטי.

---

## למה זה, ולא מה שהיה

| דרך | חינם? | שומר על הקוד פרטי? | עובד עכשיו? |
| --- | :---: | :---: | :---: |
| Runner של GitHub | ✗ בפרטי | ✓ | **✗** — נכשל תוך 3 שניות על חיוב |
| להפוך את הרפו לציבורי | ✓ | **✗** | ✓ |
| **Runner על המחשב שלך** | **✓** | **✓** | **✓** |

השורה השלישית היא מה שמותקן כאן. GitHub שולח את העבודה למחשב שלך, הוא בונה,
ומעלה את התוצאה בחזרה.

---

## מה צריך להיות מותקן

אותם דברים שהמתקין צריך ממילא:

- **Visual Studio 2022** עם `Desktop development with C++`
- **CMake** — מגיע עם Visual Studio
- **JUCE 9** ב־`%USERPROFILE%\JUCE`

אם כבר הרצת את `INSTALL-FOUR-COLOR.bat` — הכל שם.

---

## ההתקנה — פעם אחת, בערך 5 דקות

### 1. קבל את הפקודות מ־GitHub

פתח בדפדפן:

```
https://github.com/gusanaaman1-boop/FourColor/settings/actions/runners/new?arch=x64&os=win
```

הדף הזה נותן לך **טוקן אישי שתקף לשעה** ומציג בדיוק את הפקודות. אל תעתיק
טוקנים ממקום אחר — הם חד־פעמיים.

### 2. הרץ אותן ב־PowerShell

הדף ייתן משהו בסגנון הבא. **קח את הגרסה שבדף, לא את זו שכאן** — מספר הגרסה
והטוקן משתנים:

```powershell
mkdir C:\actions-runner ; cd C:\actions-runner
Invoke-WebRequest -Uri https://github.com/actions/runner/releases/download/v2.xxx.x/actions-runner-win-x64-2.xxx.x.zip -OutFile runner.zip
Expand-Archive -Path runner.zip -DestinationPath .
./config.cmd --url https://github.com/gusanaaman1-boop/FourColor --token <הטוקן מהדף>
```

בשאלות שהוא שואל:

| שאלה | מה לענות |
| --- | --- |
| runner group | Enter (ברירת מחדל) |
| name of runner | Enter, או `gusa-windows` |
| **additional labels** | **Enter** — חשוב, ברירת המחדל כוללת `Windows` ו־`X64` שה־workflow מחפש |
| work folder | Enter |

### 3. הפעל אותו כשירות

כדי שירוץ לבד בכל הפעלה של המחשב, ב־PowerShell **כמנהל**:

```powershell
cd C:\actions-runner
./svc.cmd install
./svc.cmd start
```

זהו.

---

## איך לוודא שזה עובד

1. חזור ל־`Settings → Actions → Runners` ב־GitHub. אתה אמור לראות את המחשב
   שלך עם נקודה **ירוקה** ליד השם.
2. לך ל־`Actions → windows → Run workflow`, השאר `self-hosted`, ולחץ.
3. אחרי כמה דקות, בתחתית עמוד ההרצה, תחת **Artifacts**, יופיע
   `FourColor-windows-x64` — הורד אותו.

בפנים: `FourColor.vst3`, `FourColor.exe`, והמתקין. קליק ימני על
`INSTALL-FOUR-COLOR.bat` → Run as administrator.

---

## מה שיקרה מכאן והלאה

בכל דחיפה שלי ל־`main`, המחשב שלך יבנה אוטומטית ותוך כמה דקות תהיה גרסה
מוכנה להורדה. אם הבנייה תיכשל, ההרצה תהיה אדומה ואפשר לראות בדיוק מה
הקומפיילר אמר — במקום שנגלה את זה רק כשתנסה להתקין.

---

## אם משהו לא עובד

**ההרצה תקועה על "Waiting for a runner"** — המחשב כבוי, או השירות לא רץ.
בדוק את הנקודה הירוקה בדף ה־Runners.

**נכשל ב־Configure** — Visual Studio בלי ה־C++ workload. הלוג של ההרצה אומר
את זה במפורש.

**נכשל ב־Check out JUCE** — אין גישה לרשת, או ש־git לא מותקן.

**רוצה לעצור את זה:** `cd C:\actions-runner ; ./svc.cmd stop`
ולהסיר לגמרי: `./svc.cmd uninstall` ואז `./config.cmd remove --token <טוקן חדש מהדף>`

---

## אבטחה — שווה לדעת

Runner עצמאי מריץ כל קוד שנמצא ב־workflow של הרפו. ברפו **פרטי** שרק אתה
ואני דוחפים אליו זה בסדר גמור. **אל תהפוך את הרפו לציבורי כל עוד ה־runner
רשום** — ב־repo ציבורי, כל אחד שפותח Pull Request יכול להריץ קוד על המחשב
שלך. אם תרצה להפוך אותו לציבורי, תסיר את ה־runner קודם.
