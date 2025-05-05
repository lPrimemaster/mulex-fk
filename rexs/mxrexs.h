namespace mulex
{
	bool RexWriteLockFile();
	int RexAcquireLock();
	void RexReleaseLock(int fd);
	bool RexInterruptDaemon();
}
