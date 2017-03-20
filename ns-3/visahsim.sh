./waf --run "scratch/ahsimulation/ahsimulation"\
" --NGroup=4"\
" --NRawSlotNum=5"\
" --ContentionPerRAWSlot=1"\
" --ContentionPerRAWSlotOnlyInFirstGroup=true"\
" --DataMode=\"MCS2_8\""\
" --Rho=\"100\""\
" --MaxTimeOfPacketsInQueue=1000"\
" --SimulationTime=200"\
" --TrafficInterval=1000"\
" --TrafficIntervalDeviation=1000"\
" --TrafficType=\"tcpipcamera\""\
" --IpCameraMotionPercentage=1"\
" --IpCameraMotionDuration=10"\
" --IpCameraDataRate=96"\
" --BeaconInterval=102400"\
" --MinRTO=819200"\
" --TCPConnectionTimeout=60000000"\
" --TCPSegmentSize=536"\
" --APAlwaysSchedulesForNextSlot=false"\
" --APScheduleTransmissionForNextSlotIfLessThan=5000"\
" --NRawSta=96"\
" --Nsta=1"\
" --VisualizerIP=\"10.0.2.15\""\
" --VisualizerPort=7707"\
" --VisualizerSamplingInterval=1"\
" --APPcapFile=\"appcap\""\
" --NSSFile=\"test.nss\""\
" --Name=\"test\"" 


#" --useIpv6"\

#ahsimulation
#coapsim
#aminasim
