/*
 *  shellprocess.h  -  execute a process through the shell
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

/** @file shellprocess.h - execute a process through the shell */

#include <KProcess>
#include <QQueue>
#include <QByteArray>


/**
 *  @short Enhanced KProcess to run a shell command.
 *
 *  The ShellProcess class runs a shell command and interprets the shell exit status
 *  as far as possible. It blocks execution if shell access is prohibited. It buffers
 *  data written to the process's stdin.
 *
 *  Before executing any command, ShellProcess checks whether shell commands are
 *  allowed at all. If not (e.g. if the user is running in kiosk mode), it blocks
 *  execution.
 *
 *  Derived from KProcess, this class additionally tries to interpret the shell
 *  exit status. Different shells use different exit codes. Currently, if bash or ksh
 *  report that the command could not be found or could not be executed, the NOT_FOUND
 *  status is returned.
 *
 *  Writes to the process's stdin are buffered, so that unlike with KProcess, there
 *  is no need to wait for the write to complete before writing again.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class ShellProcess : public KProcess
{
    Q_OBJECT
public:
    /** Current status of the shell process. */
    enum Status
    {
        INACTIVE,     //!< start() has not yet been called to run the command
        RUNNING,      //!< command is currently running
        SUCCESS,      //!< command appears to have exited successfully
        UNAUTHORISED, //!< shell commands are not authorised for this user
        DIED,         //!< command didn't exit cleanly, i.e. was killed or died
        NOT_FOUND,    //!< command either not found or not executable
        START_FAIL    //!< command couldn't be started for other reasons
    };

    /** Constructor.
     *  @param command The command line to be run when start() is called.
     */
    explicit ShellProcess(const QString& command);

    /** Executes the configured command.
     *  @param openMode WriteOnly for stdin only, ReadOnly for stdout/stderr only, else ReadWrite.
     */
    bool            start(OpenMode = ReadWrite);

    /** Returns the current status of the shell process. */
    Status          status() const       { return mStatus; }

    /** Returns the shell exit code. Only valid if status() == SUCCESS or NOT_FOUND. */
    int             exitCode() const     { return mExitCode; }

    /** Returns whether the command was run successfully.
     *  @return True if the command has been run and appears to have exited successfully.
     */
    bool            normalExit() const   { return mStatus == SUCCESS; }

    /** Returns the command configured to be run. */
    const QString&  command() const      { return mCommand; }

    /** Returns the error message corresponding to the command exit status.
     *  @return Error message if an error occurred. Null string if the command has not yet
     *          exited, or if the command ran successfully.
     */
    QString         errorMessage() const;

    /** Writes a string to the process's STDIN. */
    void            writeStdin(const char* buffer, int bufflen);

    /** Tell the process to exit once any outstanding STDIN strings have been written. */
    void            stdinExit();

    /** Returns whether the user is authorised to run shell commands. Shell commands may
     *  be prohibited in kiosk mode, for example.
     */
    static bool     authorised();

    /** Determines which shell to use.
     *  @return file name of shell, excluding path.
     */
    static const QByteArray& shellName()   { shellPath();  return mShellName; }

    /** Determines which shell to use.
     *  @return path name of shell.
     */
    static const QByteArray& shellPath();

    /** Splits a command line at the first non-escaped space character.
     *  If the command line STARTS with a quote, any spaces before the closing
     *  quote are ignored.
     *  @param cmdline  Updated to contain the command parameters, starting with a space.
     *  @return command line up to but not including the first space.
     */
    static QString splitCommandLine(QString& cmdline);

Q_SIGNALS:
    /** Signal emitted when the shell process execution completes. It is not emitted
     *  if start() did not attempt to start the command execution, e.g. in kiosk mode.
     */
    void  shellExited(ShellProcess*);

    /** Signal emitted when input is available from the process's stdout. */
    void  receivedStdout(ShellProcess*);

    /** Signal emitted when input is available from the process's stderr. */
    void  receivedStderr(ShellProcess*);

private Q_SLOTS:
    void  writtenStdin(qint64 bytes);
    void  stdoutReady()         { Q_EMIT receivedStdout(this); }
    void  stderrReady()         { Q_EMIT receivedStderr(this); }
    void  slotExited(int exitCode, QProcess::ExitStatus);

private:
    // Prohibit the following inherited methods
    ShellProcess&  operator<<(const QString&);
    ShellProcess&  operator<<(const QStringList&);

    static QByteArray  mShellName;         // name of shell to be used
    static QByteArray  mShellPath;         // path of shell to be used
    static bool        mInitialised;       // true once static data has been initialised
    static bool        mAuthorised;        // true if shell commands are authorised
    QString            mCommand;           // copy of command to be executed
    QQueue<QByteArray> mStdinQueue;        // queued strings to send to STDIN
    qint64             mStdinBytes {0};    // bytes still to be written from first queued string
    int                mExitCode;          // shell exit value (if mStatus == SUCCESS or NOT_FOUND)
    Status             mStatus {INACTIVE}; // current execution status
    bool               mStdinExit {false}; // exit once STDIN queue has been written
};

// vim: et sw=4:
