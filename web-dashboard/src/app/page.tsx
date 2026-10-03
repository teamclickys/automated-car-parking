"use client";

import { useEffect, useState } from "react";
import { ref, onValue } from "firebase/database";
import { db } from "@/lib/firebase";
import { Car, Clock, CreditCard, ShieldAlert, CheckCircle, Wifi, WifiOff } from "lucide-react";

// Types
type BayState = {
  status: "AVAILABLE" | "OCCUPIED";
  carNumber: number | null;
  parkingStartTime: number | null;
  paymentStatus: "WAITING_FOR_PAYMENT" | "PAYMENT_DONE" | null;
};

type EntranceState = {
  distance: number;
  gate: "OPEN" | "CLOSED";
};

type SystemState = {
  online: boolean;
  totalSpaces: number;
  occupiedSpaces: number;
  freeSpaces: number;
  parkingStatus: "AVAILABLE" | "FULL";
};

type EventsState = {
  currentEvent: string;
};

const TIME_LIMIT_SECONDS = 30;

export default function Dashboard() {
  const [system, setSystem] = useState<SystemState>({
    online: false,
    totalSpaces: 4,
    occupiedSpaces: 0,
    freeSpaces: 4,
    parkingStatus: "AVAILABLE"
  });

  const [entrance, setEntrance] = useState<EntranceState>({
    distance: 0,
    gate: "CLOSED"
  });

  const [events, setEvents] = useState<EventsState>({
    currentEvent: "WAITING_FOR_CAR"
  });

  const [bays, setBays] = useState<Record<string, BayState>>({
    bay1: { status: "AVAILABLE", carNumber: null, parkingStartTime: null, paymentStatus: null },
    bay2: { status: "AVAILABLE", carNumber: null, parkingStartTime: null, paymentStatus: null },
    bay3: { status: "AVAILABLE", carNumber: null, parkingStartTime: null, paymentStatus: null },
    bay4: { status: "AVAILABLE", carNumber: null, parkingStartTime: null, paymentStatus: null },
  });

  const [currentTime, setCurrentTime] = useState(Date.now());

  // Update current time every second for timers
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(Date.now()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Firebase Listeners
  useEffect(() => {
    const connectedRef = ref(db, ".info/connected");
    onValue(connectedRef, (snap) => {
      setSystem(prev => ({ ...prev, online: snap.val() === true }));
    });

    const systemRef = ref(db, "system");
    onValue(systemRef, (snap) => {
      if (snap.exists()) setSystem(prev => ({ ...prev, ...snap.val() }));
    });

    const entranceRef = ref(db, "entrance");
    onValue(entranceRef, (snap) => {
      if (snap.exists()) setEntrance(snap.val());
    });

    const eventsRef = ref(db, "events");
    onValue(eventsRef, (snap) => {
      if (snap.exists()) setEvents(snap.val());
    });

    const baysRef = ref(db, "parking");
    onValue(baysRef, (snap) => {
      if (snap.exists()) setBays(snap.val());
    });
  }, []);

  const calculateRemainingTime = (startTime: number | null) => {
    if (!startTime) return 0;
    const elapsed = Math.floor((currentTime - startTime) / 1000);
    return Math.max(0, TIME_LIMIT_SECONDS - elapsed);
  };

  const getTimelineSteps = () => [
    "WAITING_FOR_CAR", "CAR_APPROACHING", "GATE_OPEN", "CAR_ENTERED", 
    "GATE_CLOSED", "WAITING_FOR_PAYMENT", "PAYMENT_DONE", "ASSIGNING_SPACE", 
    "CAR_PARKED", "PARKING_TIMER_RUNNING", "TIME_EXPIRED", "CAR_MUST_BE_REMOVED", "CAR_REMOVED"
  ];

  return (
    <div className="min-h-screen bg-slate-900 text-slate-100 p-4 md:p-8 font-sans">
      <div className="max-w-7xl mx-auto space-y-6">
        
        {/* TOP SECTION: System Status */}
        <header className="bg-slate-800 rounded-xl p-6 border border-slate-700 shadow-xl flex flex-col md:flex-row justify-between items-center gap-4">
          <div>
            <h1 className="text-2xl md:text-3xl font-bold text-blue-400 tracking-tight uppercase">Smart Automated Parking System</h1>
            <div className="flex items-center mt-2 space-x-2">
              {system.online ? <Wifi className="text-green-400 w-5 h-5" /> : <WifiOff className="text-red-500 w-5 h-5" />}
              <span className={`font-semibold ${system.online ? "text-green-400" : "text-red-500"}`}>
                ESP8266: {system.online ? "ONLINE" : "OFFLINE"}
              </span>
            </div>
          </div>
          <div className="flex gap-4 text-center">
            <div className="bg-slate-700 rounded-lg p-3 px-6">
              <p className="text-slate-400 text-sm font-medium">Total Spaces</p>
              <p className="text-2xl font-bold">{system.totalSpaces}</p>
            </div>
            <div className="bg-slate-700 rounded-lg p-3 px-6">
              <p className="text-slate-400 text-sm font-medium">Occupied</p>
              <p className="text-2xl font-bold text-orange-400">{system.occupiedSpaces}</p>
            </div>
            <div className="bg-slate-700 rounded-lg p-3 px-6">
              <p className="text-slate-400 text-sm font-medium">Available</p>
              <p className="text-2xl font-bold text-green-400">{system.freeSpaces}</p>
            </div>
          </div>
          <div className={`px-6 py-3 rounded-lg font-bold text-lg border-2 ${
            system.freeSpaces > 0 ? "border-green-500 text-green-400 bg-green-500/10" : "border-red-500 text-red-400 bg-red-500/10"
          }`}>
            PARKING {system.freeSpaces > 0 ? "AVAILABLE" : "FULL"}
            {system.freeSpaces === 0 && events.currentEvent === "CAR_APPROACHING" && (
              <p className="text-xs mt-1 text-red-300">— ENTRY NOT AVAILABLE —</p>
            )}
          </div>
        </header>

        {/* ENTRANCE CONTROL */}
        <section className="bg-slate-800 rounded-xl p-6 border border-slate-700 shadow-lg">
          <h2 className="text-xl font-bold text-slate-300 mb-4 flex items-center gap-2">
            <Car className="w-6 h-6 text-blue-400" />
            Entrance Control
          </h2>
          <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
            <div className="bg-slate-700/50 p-4 rounded-lg flex flex-col justify-center items-center border border-slate-600">
              <span className="text-slate-400 font-medium mb-1">Ultrasonic Distance</span>
              <span className="text-3xl font-mono font-bold text-blue-300">{entrance.distance} cm</span>
            </div>
            <div className="bg-slate-700/50 p-4 rounded-lg flex flex-col justify-center items-center border border-slate-600">
              <span className="text-slate-400 font-medium mb-1">Vehicle Status</span>
              <span className={`text-xl font-bold ${
                entrance.distance <= 10 ? "text-yellow-400" : "text-slate-300"
              }`}>
                {entrance.distance <= 10 ? "CAR APPROACHING" : "WAITING FOR VEHICLE"}
              </span>
            </div>
            <div className="bg-slate-700/50 p-4 rounded-lg flex flex-col justify-center items-center border border-slate-600">
              <span className="text-slate-400 font-medium mb-1">Gate Status</span>
              <span className={`text-2xl font-bold ${
                entrance.gate === "OPEN" ? "text-green-400" : "text-red-400"
              }`}>
                {entrance.gate}
              </span>
            </div>
          </div>
        </section>

        {/* CURRENT PROCESS TIMELINE & ACTIVITY LOG */}
        <section className="grid grid-cols-1 lg:grid-cols-3 gap-6">
          <div className="bg-slate-800 rounded-xl p-6 border border-slate-700 shadow-lg col-span-2 overflow-x-auto">
            <h2 className="text-xl font-bold text-slate-300 mb-4">Current Vehicle Process</h2>
            <div className="flex items-center min-w-[1000px] py-4">
              {getTimelineSteps().map((step, idx) => {
                const isActive = events.currentEvent === step;
                return (
                  <div key={step} className="flex items-center">
                    <div className={`px-3 py-1.5 rounded-full text-[10px] font-bold whitespace-nowrap transition-colors ${
                      isActive ? "bg-blue-500 text-white shadow-[0_0_15px_rgba(59,130,246,0.5)] scale-110" : "bg-slate-700 text-slate-400"
                    }`}>
                      {step.replace(/_/g, " ")}
                    </div>
                    {idx < getTimelineSteps().length - 1 && (
                      <div className={`w-6 h-1 mx-1 ${isActive ? "bg-blue-400" : "bg-slate-700"}`}></div>
                    )}
                  </div>
                )
              })}
            </div>
          </div>
          
          {/* ACTIVITY LOG */}
          <div className="bg-slate-800 rounded-xl p-6 border border-slate-700 shadow-lg h-48 overflow-y-auto">
             <h2 className="text-xl font-bold text-slate-300 mb-4">Activity Log</h2>
             <div className="space-y-3">
               <div className="flex justify-between items-center text-sm border-b border-slate-700 pb-2">
                 <span className="text-slate-400">{new Date(currentTime).toLocaleTimeString()}</span>
                 <span className="font-medium text-blue-400">{events.currentEvent.replace(/_/g, " ")}</span>
               </div>
               <div className="flex justify-between items-center text-sm text-slate-500 pb-2 opacity-50">
                  <span>System listening for events...</span>
               </div>
             </div>
          </div>
        </section>

        {/* PARKING BAYS */}
        <section>
          <h2 className="text-xl font-bold text-slate-300 mb-4 flex items-center gap-2">
            <CheckCircle className="w-6 h-6 text-green-400" />
            Parking Bays
          </h2>
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
            {Object.entries(bays).map(([bayId, bay]) => {
              const isOccupied = bay.status === "OCCUPIED";
              const remTime = calculateRemainingTime(bay.parkingStartTime);
              const isExpired = isOccupied && remTime === 0;

              return (
                <div key={bayId} className={`relative rounded-xl p-6 border-2 transition-all shadow-lg flex flex-col items-center ${
                  !isOccupied 
                    ? "bg-slate-800/80 border-green-500/50 hover:border-green-400" 
                    : isExpired
                      ? "bg-red-950/40 border-red-500 animate-pulse"
                      : "bg-slate-800 border-orange-500/50"
                }`}>
                  <h3 className="text-2xl font-bold text-slate-200 uppercase tracking-widest mb-4">
                    {bayId.replace("bay", "Bay ")}
                  </h3>
                  
                  {!isOccupied ? (
                    <div className="flex flex-col items-center justify-center py-8">
                      <div className="w-16 h-16 rounded-full bg-green-500/20 flex items-center justify-center mb-3">
                        <CheckCircle className="w-8 h-8 text-green-400" />
                      </div>
                      <span className="text-green-400 font-bold text-xl tracking-wide">AVAILABLE</span>
                    </div>
                  ) : (
                    <div className="w-full flex flex-col items-center space-y-4">
                      <div className="bg-orange-500/20 text-orange-400 px-6 py-2 rounded-lg font-bold text-2xl border border-orange-500/30">
                        Car #{bay.carNumber}
                      </div>

                      {/* Payment Status */}
                      <div className="flex items-center gap-2 text-sm font-medium">
                        <CreditCard className="w-4 h-4 text-slate-400" />
                        {bay.paymentStatus === "PAYMENT_DONE" ? (
                          <span className="text-green-400">PAYMENT DONE</span>
                        ) : (
                          <span className="text-yellow-400 animate-pulse">WAITING FOR PAYMENT...</span>
                        )}
                      </div>

                      {/* Timer section */}
                      <div className={`w-full p-4 rounded-lg flex flex-col items-center border ${
                        isExpired ? "bg-red-900/50 border-red-500 text-red-200" : "bg-slate-900/50 border-slate-700 text-slate-300"
                      }`}>
                        <div className="flex items-center gap-2 mb-1">
                          <Clock className={`w-4 h-4 ${isExpired ? "text-red-400" : "text-blue-400"}`} />
                          <span className="text-xs uppercase font-bold tracking-wider opacity-80">
                            {isExpired ? "Time Expired" : "Time Remaining"}
                          </span>
                        </div>
                        <span className={`text-4xl font-mono font-bold ${
                          isExpired ? "text-red-400" : remTime <= 10 ? "text-orange-400" : "text-blue-300"
                        }`}>
                          {remTime}s
                        </span>
                      </div>

                      {/* Expiration Warning */}
                      {isExpired && (
                        <div className="w-full bg-red-600 text-white p-3 rounded-lg flex items-center justify-center gap-2 font-bold uppercase shadow-[0_0_20px_rgba(220,38,38,0.4)] mt-2">
                          <ShieldAlert className="w-5 h-5" />
                          MUST BE REMOVED
                        </div>
                      )}
                    </div>
                  )}
                </div>
              );
            })}
          </div>
        </section>
      </div>
    </div>
  );
}
