const fs = require('fs');
const { google } = require('googleapis');
const xlsx = require('xlsx');
const schedule = require('node-schedule');
const path = require('path');
const nodemailer = require('nodemailer');
require('dotenv').config();  // Load environment variables from .env file

const credentials = require('./service-account.json');

const maillist = [
  'e-mail_1',
  'e-mail_2',
  // e-mail_n
];

const SCOPES = ['https://www.googleapis.com/auth/spreadsheets'];
const auth = new google.auth.GoogleAuth({
  credentials,
  scopes: SCOPES,
});

async function fetchAndSendEmail() {
  const sheets = google.sheets({ version: 'v4', auth });
  const authClient = await auth.getClient();
  const spreadsheetId = 'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx'; // Replace with your spreadsheet ID
  const range = 'Sheet1!A:Z'; // Adjust range as necessary

  sheets.spreadsheets.values.get({
    spreadsheetId,
    range,
    auth: authClient,
  }, async (err, res) => {
    if (err) return console.log('The API returned an error: ' + err);
    const rows = res.data.values;
    if (rows.length) {
      const wb = xlsx.utils.book_new();
      const ws = xlsx.utils.aoa_to_sheet(rows);
      xlsx.utils.book_append_sheet(wb, ws, 'Sheet1');
      const folderPath = path.join(__dirname, 'DATA RC');
      if (!fs.existsSync(folderPath)) {
        fs.mkdirSync(folderPath);
      }
      const filename = `RC-${new Date().toISOString().split('T')[0]}.xlsx`;
      const filePath = path.join(folderPath, filename);
      xlsx.writeFile(wb, filePath);
      console.log(`Data saved to ${filePath}`);

      const errorCells = [];
      rows.forEach((row, rowIndex) => {
        row.forEach((cell, colIndex) => {
          if (cell === 'Error') {
            errorCells.push({ row: rowIndex + 1, col: colIndex + 1 });
          }
        });
      });

      const sheetSize = `${rows.length} rows x ${rows[0].length} columns`;
      const errorCount = errorCells.length;
      const errorDetails = errorCells.map(cell => `Row ${cell.row}, Column ${cell.col}`).join('; ');

      // Send Email
      await sendEmail(sheetSize, errorCount, errorDetails, filePath);

      // Clear the Google Sheet
      clearGoogleSheet(authClient);
    } else {
      console.log('No data found.');
    }
  });
}

function clearGoogleSheet(authClient) {
  const sheets = google.sheets({ version: 'v4', auth });
  const spreadsheetId = 'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx'; // Replace with your spreadsheet ID
  const range = 'Sheet1!A:Z'; // Adjust range as necessary

  sheets.spreadsheets.values.clear({
    spreadsheetId,
    range,
    auth: authClient,
  }, (err, res) => {
    if (err) return console.log('The API returned an error: ' + err);
    console.log('Google Sheet cleared');
  });
}

async function sendEmail(sheetSize, errorCount, errorDetails, filePath) {
  let transporter = nodemailer.createTransport({
    host: "smtp.gmail.com",
    port: 465,
    secure: true,
    auth: {
      user: process.env.EMAIL_USER,
      pass: process.env.EMAIL_PASS,
    },
  });

  const mailOptions = {
    from: '"E-MAIl"',
    to: maillist,
    subject: "Daily Google Sheet Report",
    html: `
      <h1>Google Sheet Report</h1>
      <p>Sheet Size: ${sheetSize}</p>
      <p>Error Count: ${errorCount}</p>
      <p>Error Details: ${errorDetails}</p>
      <p>Attached is the latest Google Sheet data.</p>
    `,
    attachments: [
      {
        filename: path.basename(filePath),
        path: filePath
      }
    ]
  };

  try {
    let info = await transporter.sendMail(mailOptions);
    console.log('Email sent: ' + info.response);
  } catch (error) {
    console.error('Error sending email:', error);
  }
}


// Schedule the job to run every day at 23:59:59
schedule.scheduleJob('59 23 * * *', () => {
  console.log('Running end-of-day data fetch and email...');
  fetchAndSendEmail();
});

