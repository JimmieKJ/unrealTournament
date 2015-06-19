<Query Kind="SQL">
  <Connection>
    <ID>e40cf789-b18a-4b65-bae6-a0f5440c1ce2</ID>
    <Persist>true</Persist>
    <Server>db-09</Server>
    <Database>CrashReport</Database>
    <IsProduction>true</IsProduction>
  </Connection>
  <Output>DataGrids</Output>
</Query>




--use crashreport
--delete from crashes
--where branch is null and timeofcrash > '2015-01-01'
--go


use crashreport
delete from crashes
where pattern is null and timeofcrash > '2015-01-01'
go

--use crashreport
--delete from crashes
--where branch is null and timeofcrash > '2015-01-01'
--go