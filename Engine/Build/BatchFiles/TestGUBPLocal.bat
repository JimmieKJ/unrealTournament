set uebp_testfail=0

call runuattest.bat gubp -ListOnly -cleanlocal -Allplatforms -Fake -FakeEC %*

call runuattest.bat gubp -Allplatforms -Fake -FakeEC %* -CommanderJobSetupOnly

call runuattest.bat gubp -Allplatforms -Fake -FakeEC %* -CommanderJobSetupOnly -TriggerNode=Rocket_WaitToMakeBuild

call runuattest.bat gubp -Allplatforms -Fake -FakeEC %* -CommanderJobSetupOnly -TriggerNode=Shared_WaitForPromotable

call runuattest.bat gubp -Allplatforms -Fake -FakeEC %* -CommanderJobSetupOnly -TriggerNode=Shared_WaitForTesting

call runuattest.bat gubp -Allplatforms -Fake -FakeEC %* -CommanderJobSetupOnly -TriggerNode=Shared_WaitForPromotion




if "%uebp_testfail%" == "1" goto fail

ECHO ****************** All tests Succeeded
exit /B 0

:fail
ECHO ****************** A test failed
exit /B 1



	