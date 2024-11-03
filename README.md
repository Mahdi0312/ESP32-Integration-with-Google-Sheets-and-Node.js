# ESP32-Integration-with-Google-Sheets-and-Node.js

# Explanation of the ESP32 Unit Code

The code operates as follows:
  1. It first synchronizes with the local time.
  2. Then, it connects to the Wi-Fi network.
  3. It checks the connected SHT31 and SGP30 sensors on the SDA pin, as these sensors share the same SCL clock pin.
  4. It sets up access to Google Sheets to save the data measured by the various sensors.
In the main loop, every 4 minutes (sampling frequency defined according to our needs), the unit measures different parameters via the sensors, formats them in JSON, and then sends them to Google Sheets.

To retrieve this data, we developed a server with Node.js, launched on a PC with Git Bash. This server retrieves all the information stored in Google Sheets and saves it on the PC under the following name format: 'RC-YY-MM-DD'.

# Server Code ('Server.js')

Key Points:

  1. Data Retrieval and Processing:
  The fetchAndSendEmail function retrieves data from the Google Sheet, saves it to an Excel file, and searches for any cells with the value "Error." It constructs the email content, including the sheet size, the count of "Error" cells, and details about the locations of these errors.
  2. Sending an Email:
  The sendEmail function sends an email with the collected information and attaches the Excel file.
  3. Task Scheduling:
  The task is scheduled to run every day at 11:59:59 PM using node-schedule.
  4. Clearing the Google Sheet:
  The clearGoogleSheet function clears the Google Sheet after the data has been retrieved and processed.  
  5. Environment Variables:
  Ensure that email credentials are set correctly in a .env file with the following:  
  export EMAIL_USER = E-MAIL
  export EMAIL_PASS = XXXX XXXX XXXX XXXX 
  6. Running the Script:
  Run the script using $ node server.js. The script will run daily at 11:59:59 PM, retrieve data from the Google Sheet, check for errors, and send an email with the relevant information and the attached Excel file.

To execute the code, navigate to the server code directory, right-click, and select "Git Bash Here" to launch Git Bash.

Once all the data is saved in a file named 'RC-YY-MM-DD', a command is executed to delete all previous information in Google Sheets, preparing it for a new backup. This process runs daily at midnight, at 11:59:59 PM, with the command:
