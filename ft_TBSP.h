/*-------------------------------------------------------
FAST-TrIPs: Flexible Assignment and Simulation Tool for Transit and Intermodal Passengers
Copyright (C) 2013 by Alireza Khani
Released under the GNU General Public License, version 2.
-------------------------------------------------------
Code primarily written by Alireza Khani
Under supervision of Mark Hickman

Contact:
    Alireza Khani:  akhani@utexas.edu or akhani@email.arizona.edu
    Mark Hickman:   m.hickman1@uq.edu.au
-------------------------------------------------------
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
-------------------------------------------------------*/

#include <time.h>
#include <stdlib.h>
using namespace std;

//////////////////////////////////////////////////////////////////////////////////////////////////////////
int			forwardTBSP(string _origin, string _destination, double _PDT, double _PAT, int _timeBuffer, int _threadId){
	int						i, j, numIterations, permanentLabel;
	int						tmpNumAccess, tmpNumTransfers, tmpSeqNum, tmpMaxSeqNum, tmpResidualCapacity;
	double					tmpCurrentLabel, tmpCurrentArrival, tmpOldLabel, tmpNewLabel;
	double					tmpAccessTime, tmpTransferTime, tmpNewDeparture, tmpNewArrival, tmpInVehTime, tmpWaitingTime;
	string					buf, tmpStr, tmpQueuvalue, tmpCurrentStop, tmpNewStop, tmpAccessibleTrips, tmpTrip, tmpMode, tmpFromToAt;
	char					chr[99];
	vector<string>			tokens;
	list<taz*>::iterator	tmpTazListIter;
	taz*					tmpTazPntr;
	list<stop*>::iterator	tmpStopListIter;
	stop*					tmpStopPntr;
	list<trip*>::iterator	tmpTripListIter;
	trip*					tmpTripPntr;
	priority_queue<string>	stopQueue;

	//Initialization
	for(tmpTazListIter=tazList.begin();tmpTazListIter!=tazList.end();tmpTazListIter++){
		tmpTazPntr = NULL;
		tmpTazPntr = *tmpTazListIter;
		tmpTazPntr->resetTaz(_threadId);
	}
	for(tmpStopListIter=stopList.begin();tmpStopListIter!=stopList.end();tmpStopListIter++){
		tmpStopPntr = NULL;
		tmpStopPntr = *tmpStopListIter;
		tmpStopPntr->resetStop(_threadId);
	}
	for(tmpTripListIter=tripList.begin();tmpTripListIter!=tripList.end();tmpTripListIter++){
		tmpTripPntr = NULL;
		tmpTripPntr = *tmpTripListIter;
		tmpTripPntr->resetTripUsedBefore(_threadId);
	}
	stopQueue.empty();

	//Access From Origin TAZ
	tmpTazPntr = NULL;
	tmpTazPntr = tazSet[_origin];
	tmpTazPntr->forwardUpdate(0, _PDT, "Start", _threadId);
	tmpNumAccess = tmpTazPntr->getNumStops();
	for(i=0;i<tmpNumAccess;i++){
		tmpNewStop = tmpTazPntr->getStop(i);
		tmpAccessTime = tmpTazPntr->getAccessTime(i);
		tmpNewLabel = tmpAccessTime;
		tmpNewArrival = _PDT + tmpAccessTime;
		stopSet[tmpNewStop]->forwardUpdate(tmpNewLabel, tmpNewArrival, "Access", _origin, _threadId);
		tmpQueuvalue = itoa(int(999999-tmpNewLabel*1000),chr,10);
		tmpStr.resize(6-tmpQueuvalue.length(),'0');
		tmpQueuvalue = tmpStr + tmpQueuvalue + tmpNewStop;
		stopQueue.push(tmpQueuvalue);
	}

	//Labeling loop
	numIterations = 0;
	while(stopQueue.size()>0){
		tmpStr = stopQueue.top();
		stopQueue.pop();
		tmpCurrentStop = tmpStr.substr(6,99);
		tmpStopPntr = NULL;
		tmpStopPntr = stopSet[tmpCurrentStop];
		tmpCurrentLabel = tmpStopPntr->getLabel(_threadId);
		tmpCurrentArrival = tmpStopPntr->getArrival(_threadId);
		tmpMode = tmpStopPntr->getArrivalTripId(_threadId);
		permanentLabel = tmpStopPntr->getPermanentLabel(_threadId);
		if(permanentLabel==1){
			continue;
		}else{
			stopSet[tmpCurrentStop]->setPermanentLabel(1, _threadId);
		}

		//Update by Transfers
		if(tmpMode.substr(0,1)=="t"){															//To avoid transfer right after access/egress/transfer
			tmpNumTransfers = tmpStopPntr->getNumTransfers();
			for(i=0;i<tmpNumTransfers;i++){
				tmpNewStop = tmpStopPntr->getTransfer(i);
				tmpTransferTime = tmpStopPntr->getTransferTime(i);
				tmpNewLabel = tmpCurrentLabel + tmpTransferTime;
				tmpOldLabel = stopSet[tmpNewStop]->getLabel(_threadId);
				if(tmpNewLabel < tmpOldLabel && tmpNewLabel < 999 && tmpNewLabel > 0){
					tmpNewArrival = tmpStopPntr->getArrival(_threadId) + tmpTransferTime;
					stopSet[tmpNewStop]->forwardUpdate(tmpNewLabel, tmpNewArrival, "Transfer", tmpCurrentStop, _threadId);
					tmpQueuvalue = itoa(int(999999-tmpNewLabel*1000),chr,10);
					tmpStr.resize(6-tmpQueuvalue.length(),'0');
					tmpQueuvalue = tmpStr + tmpQueuvalue + tmpNewStop;
					stopQueue.push(tmpQueuvalue);
				}
			}
		}

		//Update by Trips
		tmpAccessibleTrips = tmpStopPntr->getForwardAccessibleTrips(tmpCurrentArrival, _timeBuffer);
		buf.clear();
		tokens.clear();
		stringstream	ss(tmpAccessibleTrips);
		while (ss >> buf){
			tokens.push_back(buf);
		}
		for(i=0;i<tokens.size();i=i+2){
			tmpTrip = tokens[i];
			tmpSeqNum = atoi(tokens[i+1].c_str());
			tmpTripPntr = tripSet[tmpTrip];
			if (tmpTripPntr->getTripUsedBefore(_threadId)==1){
				continue;
			}else{
                tmpTripPntr->setTripUsedBefore(_threadId);
            }
			tmpResidualCapacity = tmpTripPntr->getResidualCapacity(tmpSeqNum);
			if(tmpResidualCapacity<1){
			//	continue;
			}
            tmpFromToAt = tmpTrip + "," + tmpCurrentStop;
            if(availableCapacity.find(tmpFromToAt)!=availableCapacity.end()){
                if(availableCapacity[tmpFromToAt]==0){
                    continue;
                }
            }
			tmpMaxSeqNum = tmpTripPntr->getMaxSequence();
			for(j=tmpSeqNum+1;j<=tmpMaxSeqNum;j++){
				tmpNewDeparture = tmpTripPntr->getSchDeparture(tmpSeqNum);
				tmpNewArrival = tmpTripPntr->getSchArrival(j);
				tmpInVehTime = tmpNewArrival - tmpNewDeparture;
				tmpNewLabel = tmpCurrentLabel + tmpInVehTime + tmpWaitingTime;
				tmpNewStop = tmpTripPntr->getStop(j);
				tmpOldLabel = stopSet[tmpNewStop]->getLabel(_threadId);
				if(tmpNewLabel < tmpOldLabel && tmpNewLabel < 999 && tmpNewLabel > 0){
					stopSet[tmpNewStop]->forwardUpdate(tmpNewLabel, tmpNewArrival, tmpTrip, tmpCurrentStop, _threadId);
					tmpQueuvalue = itoa(int(999999-tmpNewLabel*1000),chr,10);
					tmpStr.resize(6-tmpQueuvalue.length(),'0');
					tmpQueuvalue = tmpStr + tmpQueuvalue + tmpNewStop;
					stopQueue.push(tmpQueuvalue);
				}
			}
		}
		numIterations++;
	}

	//Connect to the Destination TAZ
    tmpTazPntr = tazSet[_destination];
    tmpNumAccess = tmpTazPntr->getNumStops();
    for(i=0;i<tmpNumAccess;i++){
        tmpOldLabel = tmpTazPntr->getLabel(_threadId);
        tmpNewStop = tmpTazPntr->getStop(i);
        tmpAccessTime = tmpTazPntr->getAccessTime(i);
        tmpNewLabel = stopSet[tmpNewStop]->getLabel(_threadId) + tmpAccessTime;
        tmpNewArrival = stopSet[tmpNewStop]->getArrival(_threadId) + tmpAccessTime;
        tmpMode = stopSet[tmpNewStop]->getArrivalTripId(_threadId);
        if(tmpMode.substr(0,1)=="t"){
            if(tmpNewLabel < tmpOldLabel){
                tmpTazPntr->forwardUpdate(tmpNewLabel, tmpNewArrival, tmpNewStop, _threadId);
            }
        }
    }
	return numIterations;
}
int			backwardTBSP(string _origin, string _destination, double _PDT, double _PAT, int _timeBuffer, int _threadId){
	int						i, j, numIterations, permanentLabel;
	int						tmpNumAccess, tmpNumTransfers, tmpSeqNum, tmpResidualCapacity;
	double					tmpCurrentLabel, tmpCurrentDeparture, tmpOldLabel, tmpNewLabel;
	double					tmpAccessTime, tmpTransferTime, tmpNewDeparture, tmpNewArrival, tmpInVehTime, tmpWaitingTime;
	string					buf, tmpStr, tmpQueuvalue, tmpCurrentStop, tmpNewStop, tmpAccessibleTrips, tmpTrip, tmpMode, tmpFromToAt;
	char					chr[99];
	vector<string>			tokens;
	list<taz*>::iterator	tmpTazListIter;
	taz*					tmpTazPntr;
	list<stop*>::iterator	tmpStopListIter;
	stop*					tmpStopPntr;
	list<trip*>::iterator	tmpTripListIter;
	trip*					tmpTripPntr;
	priority_queue<string>	stopQueue;

	//Initialization
	for(tmpTazListIter=tazList.begin();tmpTazListIter!=tazList.end();tmpTazListIter++){
		tmpTazPntr = NULL;
		tmpTazPntr = *tmpTazListIter;
		tmpTazPntr->resetTaz(_threadId);
	}
	for(tmpStopListIter=stopList.begin();tmpStopListIter!=stopList.end();tmpStopListIter++){
		tmpStopPntr = NULL;
		tmpStopPntr = *tmpStopListIter;
		tmpStopPntr->resetStop(_threadId);
	}
	for(tmpTripListIter=tripList.begin();tmpTripListIter!=tripList.end();tmpTripListIter++){
		tmpTripPntr = NULL;
		tmpTripPntr = *tmpTripListIter;
		tmpTripPntr->resetTripUsedBefore(_threadId);
	}
	stopQueue.empty();

	//Egress to Destination TAZ
	tmpTazPntr = NULL;
	tmpTazPntr = tazSet[_destination];
	tmpTazPntr->backwardUpdate(0, _PAT, "End", _threadId);
	tmpNumAccess = tmpTazPntr->getNumStops();
	for(i=0;i<tmpNumAccess;i++){
		tmpNewStop = tmpTazPntr->getStop(i);
		tmpAccessTime = tmpTazPntr->getAccessTime(i);
		tmpNewLabel = tmpAccessTime;
		tmpNewDeparture = _PAT - tmpAccessTime;
		stopSet[tmpNewStop]->backwardUpdate(tmpNewLabel, tmpNewDeparture, "Egress", _destination, _threadId);
		tmpQueuvalue = itoa(int(999999-tmpNewLabel*1000),chr,10);
		tmpStr.resize(6-tmpQueuvalue.length(),'0');
		tmpQueuvalue = tmpStr + tmpQueuvalue + tmpNewStop;
		stopQueue.push(tmpQueuvalue);
	}

	//Labeling loop
	numIterations = 0;
	while(stopQueue.size()>0){
		tmpStr = stopQueue.top();
		stopQueue.pop();
		tmpCurrentStop = tmpStr.substr(6,99);
		tmpStopPntr = NULL;
		tmpStopPntr = stopSet[tmpCurrentStop];
		tmpCurrentLabel = tmpStopPntr->getLabel(_threadId);
		tmpCurrentDeparture = tmpStopPntr->getDeparture(_threadId);
		tmpMode = tmpStopPntr->getDepartureTripId(_threadId);
		permanentLabel = tmpStopPntr->getPermanentLabel(_threadId);
		if(permanentLabel==1){
			continue;
		}else{
			stopSet[tmpCurrentStop]->setPermanentLabel(1, _threadId);
		}
        //tmpFromToAt = tmpMode + "," + tmpCurrentStop;

		//Update by Transfers
		if(tmpMode.substr(0,1)=="t"){																//To avoid transfer right after access/egress/transfer
			tmpNumTransfers = tmpStopPntr->getNumTransfers();
			for(i=0;i<tmpNumTransfers;i++){
				tmpNewStop = tmpStopPntr->getTransfer(i);
                tmpTransferTime = tmpStopPntr->getTransferTime(i);
				tmpNewLabel = tmpCurrentLabel + tmpTransferTime;
				tmpOldLabel = stopSet[tmpNewStop]->getLabel(_threadId);
				tmpNewDeparture = tmpCurrentDeparture - tmpTransferTime;
				if(tmpNewLabel < tmpOldLabel && tmpNewLabel < 999 && tmpNewLabel > 0){
					stopSet[tmpNewStop]->backwardUpdate(tmpNewLabel, tmpNewDeparture, "Transfer", tmpCurrentStop, _threadId);
					tmpQueuvalue = itoa(int(999999-tmpNewLabel*1000),chr,10);
					tmpStr.resize(6-tmpQueuvalue.length(),'0');
					tmpQueuvalue = tmpStr + tmpQueuvalue + tmpNewStop;
					stopQueue.push(tmpQueuvalue);
				}
			}
		}

		//Update by Trips
		tmpAccessibleTrips = tmpStopPntr->getBackwardAccessibleTrips(tmpCurrentDeparture, _timeBuffer);
		buf.clear();
		tokens.clear();
		stringstream	ss(tmpAccessibleTrips);
		while (ss >> buf){
			tokens.push_back(buf);
		}
		for(i=0;i<tokens.size();i=i+2){
			tmpTrip = tokens[i];
			tmpSeqNum = atoi(tokens[i+1].c_str());
			tmpTripPntr = tripSet[tmpTrip];
			if (tmpTripPntr->getTripUsedBefore(_threadId)==1){
				continue;
			}else{
                tmpTripPntr->setTripUsedBefore(_threadId);
            }
			for(j=1;j<tmpSeqNum;j++){
				tmpResidualCapacity = tmpTripPntr->getResidualCapacity(j);
				if(tmpResidualCapacity<1){
					continue;
				}
                tmpNewDeparture = tmpTripPntr->getSchDeparture(j);
				tmpNewArrival = tmpTripPntr->getSchArrival(tmpSeqNum);
				tmpInVehTime = tmpNewArrival - tmpNewDeparture;
                tmpWaitingTime = tmpCurrentDeparture - tmpNewArrival;
				tmpNewLabel = tmpCurrentLabel + tmpInVehTime + tmpWaitingTime ;
				tmpNewStop = tmpTripPntr->getStop(j);
                tmpFromToAt = tmpTrip + "," + tmpNewStop;
                if(availableCapacity.find(tmpFromToAt)!=availableCapacity.end()){
                    if(availableCapacity[tmpFromToAt]==0){
                //        continue;
                    }
                }
				tmpOldLabel = stopSet[tmpNewStop]->getLabel(_threadId);
				if(tmpNewLabel < tmpOldLabel && tmpNewLabel < 999 && tmpNewLabel > 0){
					stopSet[tmpNewStop]->backwardUpdate(tmpNewLabel, tmpNewDeparture, tmpTrip, tmpCurrentStop, _threadId);
					tmpQueuvalue = itoa(int(999999-tmpNewLabel*1000),chr,10);
					tmpStr.resize(6-tmpQueuvalue.length(),'0');
					tmpQueuvalue = tmpStr + tmpQueuvalue + tmpNewStop;
					stopQueue.push(tmpQueuvalue);
				}
			}
		}
		numIterations++;
	}

	//Connect to the Origin TAZ
    tmpTazPntr=tazSet[_origin];
    tmpNumAccess = tmpTazPntr->getNumStops();
    for(i=0;i<tmpNumAccess;i++){
        tmpOldLabel = tmpTazPntr->getLabel(_threadId);
        tmpNewStop = tmpTazPntr->getStop(i);
        tmpAccessTime = tmpTazPntr->getAccessTime(i);
        tmpNewLabel = stopSet[tmpNewStop]->getLabel(_threadId) + tmpAccessTime;
        tmpNewDeparture = stopSet[tmpNewStop]->getDeparture(_threadId) - tmpAccessTime;
        tmpMode = stopSet[tmpNewStop]->getDepartureTripId(_threadId);
        //tmpFromToAt = tmpMode + "," + tmpNewStop;
        if(tmpMode.substr(0,1)=="t"){
            if(tmpNewLabel < tmpOldLabel){
                tmpTazPntr->backwardUpdate(tmpNewLabel, tmpNewDeparture, tmpNewStop, _threadId);
            }
        }
    }
	return numIterations;
}
string		getForwardPath(string _origin, string _destination, double _PDT, double _PAT, int _threadId){
	int			tmpStrLen;
	string		tmpIn, tmpCurrentStop, tmpNewStop, tmpCurrentTrip, tmpAccessLink, tmpTransferLink, tmpLastTrip, tmpFirstTrip, tmpFirstStop;
	double		tmpStartTime, tmpTransferTime;
	string		tmpStr, tmpBoardingStops, tmpAlightingStops, tmpTrips, tmpWalkingTimes, tmpPath, tmpDepartureTime;
	char		chr[99];

	tmpCurrentStop = tazSet[_destination]->getPredecessor(_threadId);
	if(tmpCurrentStop=="-101"){
		return "-101";
	}
	tmpCurrentTrip = stopSet[tmpCurrentStop]->getArrivalTripId(_threadId);
	if(tmpCurrentTrip.substr(0,1)=="t"){
		tmpFirstTrip = tmpCurrentTrip;
	}

	tmpAccessLink = _destination + "," + tmpCurrentStop;
	tmpIn = itoa(int(100*accessTimes[tmpAccessLink]),chr,10);											//cout <<tmpIn<<endl;
	tmpStrLen = tmpIn.length();
	tmpWalkingTimes = tmpIn.substr(0,max(0,tmpStrLen-2)) + ".";
	if(tmpStrLen<2)				tmpWalkingTimes = tmpWalkingTimes + "0";
	tmpWalkingTimes = tmpWalkingTimes + tmpIn.substr(max(0,tmpStrLen-2),2);

	while(1){
		tmpNewStop = stopSet[tmpCurrentStop]->getPredecessor(_threadId);
		if(tmpNewStop.substr(0,1)=="s"){
			tmpFirstStop = tmpNewStop;
		}
		if(tmpCurrentTrip=="Access"){
			if(tmpTrips==""){
				return "-101";
			}
			tmpStr = tmpWalkingTimes;
			tmpAccessLink = tmpNewStop + "," + tmpCurrentStop;
			tmpIn = itoa(int(100*accessTimes[tmpAccessLink]),chr,10);											//cout <<tmpIn<<endl;
			tmpStrLen = tmpIn.length();
			tmpWalkingTimes = tmpIn.substr(0,max(0,tmpStrLen-2)) + ".";
			if(tmpStrLen<2)				tmpWalkingTimes = tmpWalkingTimes + "0";
			tmpWalkingTimes = tmpWalkingTimes + tmpIn.substr(max(0,tmpStrLen-2),2);								//cout <<tmpWalkingTimes<<endl;
			tmpWalkingTimes.append(",");
			tmpWalkingTimes.append(tmpStr);

			tmpStartTime = tripSet[tmpFirstTrip]->getSchDepartureByStop(tmpFirstStop) - accessTimes[tmpAccessLink];

			tmpIn = itoa(int(100*tmpStartTime),chr,10);											//cout <<tmpIn<<endl;
			tmpStrLen = tmpIn.length();
			tmpPath = tmpIn.substr(0,max(0,tmpStrLen-2)) + ".";
			if(tmpStrLen<2)				tmpPath = tmpPath + "0";
			tmpPath = tmpPath + tmpIn.substr(max(0,tmpStrLen-2),2);								//cout <<tmpWalkingTimes<<endl;

			tmpPath.append("\t");
			tmpPath.append(tmpBoardingStops);
			tmpPath.append("\t");
			tmpPath.append(tmpTrips);
			tmpPath.append("\t");
			tmpPath.append(tmpAlightingStops);
			tmpPath.append("\t");
			tmpPath.append(tmpWalkingTimes);									//cout <<tmpPath<<endl;
			return tmpPath;
		}else if(tmpCurrentTrip=="Transfer"){
			tmpTransferLink = tmpNewStop + "," + tmpCurrentStop;

			tmpStr = tmpWalkingTimes;
			tmpIn = itoa(int(100*transferTimes[tmpTransferLink]),chr,10);											//cout <<tmpIn<<endl;
			tmpStrLen = tmpIn.length();
			tmpWalkingTimes = tmpIn.substr(0,max(0,tmpStrLen-2)) + ".";
			if(tmpStrLen<2)				tmpWalkingTimes = tmpWalkingTimes + "0";
			tmpWalkingTimes = tmpWalkingTimes + tmpIn.substr(max(0,tmpStrLen-2),2);								//cout <<tmpWalkingTimes<<endl;
			tmpWalkingTimes.append(",");
			tmpWalkingTimes.append(tmpStr);

			tmpLastTrip = tmpCurrentTrip;
		}else if(tmpCurrentTrip.substr(0,1)=="t"){
			tmpStr = tmpBoardingStops;
			tmpBoardingStops = tmpNewStop;//.substr(1,99);
			if(tmpStr!=""){
				tmpBoardingStops.append(",");
			}
			tmpBoardingStops.append(tmpStr);

			tmpStr = tmpAlightingStops;
			tmpAlightingStops = tmpCurrentStop;//.substr(1,99);
			if(tmpStr!=""){
				tmpAlightingStops.append(",");
			}
			tmpAlightingStops.append(tmpStr);

			tmpStr = tmpTrips;
			tmpTrips = tmpCurrentTrip;//.substr(1,99);
			if(tmpStr!=""){
				tmpTrips.append(",");
			}
			tmpTrips.append(tmpStr);
			if(tmpLastTrip.substr(0,1)=="t"){
				tmpStr = tmpWalkingTimes;
				tmpWalkingTimes = "0,";
				tmpWalkingTimes.append(tmpStr);
			}
			tmpLastTrip = tmpCurrentTrip;
		}
		tmpCurrentStop = tmpNewStop;
		tmpCurrentTrip = stopSet[tmpCurrentStop]->getArrivalTripId(_threadId);
		if(tmpCurrentTrip.substr(0,1)=="t"){
			tmpFirstTrip = tmpCurrentTrip;
		}
	}
	return "-101";
}
string		getBackwardPath(string _origin, string _destination, double _PDT, double _PAT, int _threadId){
	int			tmpStrLen, tmpStopSeq;
	string		tmpIn, tmpStr, tmpCurrentStop, tmpNewStop, tmpCurrentTrip, tmpAccessLink, tmpTransferLink, tmpLastTrip;
	double		tmpStartTime, tmpTransferTime, tmpHeadway;
	string		tmpBoardingStops, tmpAlightingStops, tmpTrips, tmpWalkingTimes, tmpPath;
	char		chr[99];

	tmpCurrentStop = tazSet[_origin]->getSuccessor(_threadId);
	if(tmpCurrentStop=="-101"){
		return "-101";
	}
	tmpCurrentTrip = stopSet[tmpCurrentStop]->getDepartureTripId(_threadId);
	tmpStartTime = tazSet[_origin]->getDeparture(_threadId);

	tmpIn = itoa(int(100*tmpStartTime),chr,10);
	tmpStrLen = tmpIn.length();
	tmpPath = tmpIn.substr(0,max(0,tmpStrLen-2)) + ".";
	if(tmpStrLen<2)			tmpPath = tmpPath + "0";
	tmpPath = tmpPath + tmpIn.substr(max(0,tmpStrLen-2),2);

	tmpAccessLink = _origin + "," + tmpCurrentStop;
	tmpIn = itoa(int(100*accessTimes[tmpAccessLink]),chr,10);
	tmpStrLen = tmpIn.length();
	tmpWalkingTimes = tmpWalkingTimes + tmpIn.substr(0,max(0,tmpStrLen-2)) + ".";
	if(tmpStrLen<2)			tmpWalkingTimes = tmpWalkingTimes + "0";
	tmpWalkingTimes = tmpWalkingTimes + tmpIn.substr(max(0,tmpStrLen-2),2);

	while(1){
		if(tmpCurrentTrip=="-101"){
			return "-101";
		}else if(tmpCurrentTrip=="Egress"){
			if(tmpTrips==""){
				return "-101";
			}
			tmpNewStop = stopSet[tmpCurrentStop]->getSuccessor(_threadId);
			tmpAccessLink = tmpNewStop + "," + tmpCurrentStop;
			tmpIn = itoa(int(100*accessTimes[tmpAccessLink]),chr,10);											//cout <<tmpIn<<endl;
			tmpStrLen = tmpIn.length();
			tmpWalkingTimes = tmpWalkingTimes + "," + tmpIn.substr(0,max(0,tmpStrLen-2)) + ".";
			if(tmpStrLen<2)			tmpWalkingTimes = tmpWalkingTimes + "0";
			tmpWalkingTimes = tmpWalkingTimes + tmpIn.substr(max(0,tmpStrLen-2),2);								//cout <<tmpWalkingTimes<<endl;

			tmpPath.append("\t");
			tmpPath.append(tmpBoardingStops);
			tmpPath.append("\t");
			tmpPath.append(tmpTrips);
			tmpPath.append("\t");
			tmpPath.append(tmpAlightingStops);
			tmpPath.append("\t");
			tmpPath.append(tmpWalkingTimes);
			return tmpPath;
		}else if(tmpCurrentTrip=="Transfer"){
			tmpNewStop = stopSet[tmpCurrentStop]->getSuccessor(_threadId);
			tmpTransferLink = tmpCurrentStop + "," + tmpNewStop;

			tmpIn = itoa(int(100*transferTimes[tmpTransferLink]),chr,10);											//cout <<tmpIn<<endl;
			tmpStrLen = tmpIn.length();
			tmpWalkingTimes = tmpWalkingTimes + "," + tmpIn.substr(0,max(0,tmpStrLen-2)) + ".";
			if(tmpStrLen<2)			tmpWalkingTimes = tmpWalkingTimes + "0";
			tmpWalkingTimes = tmpWalkingTimes + tmpIn.substr(max(0,tmpStrLen-2),2);								//cout <<tmpWalkingTimes<<endl;

			tmpLastTrip = tmpCurrentTrip;
		}else if(tmpCurrentTrip.substr(0,1)=="t"){
			if(tmpBoardingStops!=""){
				tmpBoardingStops.append(",");
			}
			tmpBoardingStops.append(tmpCurrentStop);//.substr(1,99));

			tmpNewStop = stopSet[tmpCurrentStop]->getSuccessor(_threadId);
			if(tmpAlightingStops!=""){
				tmpAlightingStops.append(",");
			}
			tmpAlightingStops.append(tmpNewStop);//.substr(1,99));

			if(tmpTrips!=""){
				tmpTrips.append(",");
			}
			tmpTrips.append(tmpCurrentTrip);//.substr(1,99));
			if(tmpLastTrip.substr(0,1)=="t"){
				tmpWalkingTimes.append(",0");
			}
			tmpLastTrip = tmpCurrentTrip;
		}
		tmpCurrentStop = tmpNewStop;
		tmpCurrentTrip = stopSet[tmpCurrentStop]->getDepartureTripId(_threadId);
	}
	return "-101";
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int	disaggregateDeterministicAssignment(int _timeBuff, int _numThreads){
	int									k, numThreads, tmpNumPassengers, tmpNumPtahs;
	clock_t								t_start;

	numThreads = _numThreads;
	parallelizeStops(numThreads);
	parallelizeTazs(numThreads);
	parallelizeTrips(numThreads);

	t_start = clock();
	cout <<"**************************** GENERATING PATHS ****************************"<<endl;
	tmpNumPassengers = passengerSet.size();
	tmpNumPtahs = 0;
	for(k=0;k<tmpNumPassengers;k++){
		int					threadId, tmpNumIterations, tmpTourHalf, tmpStatus;
		string				tmpPassengerId, tmpOriginTaz, tmpDestinationTaz, tmpPath;
		double				tmpPDT, tmpPAT;
		passenger*			passengerPntr;
		map<string,passenger*>::iterator	tmpPassengerIter;

		threadId = 0;
		tmpPassengerIter = passengerSet.begin();
		advance(tmpPassengerIter, k);
		if(tmpPassengerIter==passengerSet.end())	continue;

		tmpPassengerId = (*tmpPassengerIter).first;
		passengerPntr = NULL;
		passengerPntr = passengerSet[tmpPassengerId];
		tmpOriginTaz =passengerPntr->getOriginTAZ();
		tmpDestinationTaz = passengerPntr->getDestinationTAZ();
		if(tazSet.find(tmpOriginTaz)==tazSet.end() || tazSet.find(tmpDestinationTaz)==tazSet.end())	continue;
		if(tmpOriginTaz==tmpDestinationTaz)	continue;

		tmpStatus = passengerPntr->getPassengerStatus();
        if(tmpStatus==5){
            tmpNumPtahs++;
            continue;
        }
		tmpPDT = passengerPntr->getPDT();
		tmpPAT = passengerPntr->getPAT();
		tmpTourHalf = passengerPntr->getTourHalf();
		if(tmpTourHalf==1){
			tmpNumIterations = backwardTBSP(tmpOriginTaz, tmpDestinationTaz, tmpPDT, tmpPAT, _timeBuff, threadId);
			tmpPath = getBackwardPath(tmpOriginTaz, tmpDestinationTaz, tmpPDT, tmpPAT, threadId);
		}else if(tmpTourHalf==2){
			tmpNumIterations = forwardTBSP(tmpOriginTaz, tmpDestinationTaz, tmpPDT, tmpPAT, _timeBuff, threadId);
			tmpPath = getForwardPath(tmpOriginTaz, tmpDestinationTaz, tmpPDT, tmpPAT, threadId);
		}
		if(tmpPath!="-101"){
			passengerPntr->setAssignedPath(tmpPath);
			tmpNumPtahs++;
		}
		if(k%max(min(tmpNumPassengers/10,1000),10)==0){
			cout <<k<<"\t/\t"<<tmpNumPassengers<<"\tpassengers assigned;\ttime elapsed:\t"<<difftime(clock(),t_start)/CLOCKS_PER_SEC<<endl;
		}
	}
    cout <<k<<"\t/\t"<<tmpNumPassengers<<"\tpassengers assigned;\ttime elapsed:\t"<<difftime(clock(),t_start)/CLOCKS_PER_SEC<<endl;
	return tmpNumPtahs;
}