with x as (select NGroup, NRawSlotNum, max(ContentionPerRAWSlot) as MaxPossibleContention from results
where TrafficInterval = 10000 and SensorMeasurementSize = 154 and EDCAQueueLength_max <= 1 and TCPConnected_min = 1
group by NGroup, NRawSlotNum

),

y as (
select 

NRawSlotNum as 'RAW slots',

max(case when NGroup = 1 then MaxPossibleContention end) as "1",
max(case when NGroup = 2 then MaxPossibleContention end) as "2",
max(case when NGroup = 4 then MaxPossibleContention end) as "4",
max(case when NGroup = 8 then MaxPossibleContention end) as "8",
max(case when NGroup = 12 then MaxPossibleContention end) as "12",
max(case when NGroup = 16 then MaxPossibleContention end) as "16"

from x
group by NRawSlotNum)

select  * from y

