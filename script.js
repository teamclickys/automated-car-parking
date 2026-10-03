// =====================================================================
// FIREBASE CONFIGURATION
// =====================================================================
const firebaseConfig = {
  apiKey: "AIzaSyCFH_hNAqgKBvbAXpD9vJ5zYOAXD_J3CPQ",
  authDomain: "smartparking-3f3b9.firebaseapp.com",
  databaseURL: "https://smartparking-3f3b9-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "smartparking-3f3b9",
  storageBucket: "smartparking-3f3b9.firebasestorage.app",
  messagingSenderId: "744317800260",
  appId: "1:744317800260:web:899dbd80094ccb4bd51111"
};

// =====================================================================
// INITIALIZATION
// =====================================================================
const statusElement = document.getElementById('firebase-status');

try {
    // Initialize Firebase (Compat mode)
    firebase.initializeApp(firebaseConfig);
    const db = firebase.database();
    
    statusElement.textContent = "Listening for Real-time Updates...";
    statusElement.parentElement.classList.add('connected');
    statusElement.parentElement.classList.remove('error');
    
    // Start listening to the 'parking' node
    const parkingRef = db.ref('parking');
    
    parkingRef.on('value', (snapshot) => {
        const data = snapshot.val();
        
        if (data) {
            updateDashboard(data);
            updateTimestamp();
        } else {
            console.log("No data available at /parking yet.");
        }
    }, (error) => {
        console.error("Error reading Firebase data:", error);
        statusElement.textContent = "Connection Error";
        statusElement.parentElement.classList.add('error');
        statusElement.parentElement.classList.remove('connected');
    });

} catch (error) {
    console.error("Firebase Initialization Error:", error);
    statusElement.textContent = "Firebase Error. Check configuration.";
    statusElement.parentElement.classList.add('error');
    statusElement.parentElement.classList.remove('connected');
}

// =====================================================================
// DASHBOARD UPDATE LOGIC
// =====================================================================
function updateDashboard(data) {
    // 1. Update Summary Cards
    const free = data.freeSpaces !== undefined ? data.freeSpaces : '-';
    const occ = data.occupiedSpaces !== undefined ? data.occupiedSpaces : '-';
    const total = data.totalSpaces !== undefined ? data.totalSpaces : 4;

    document.getElementById('available-spaces').textContent = free;
    document.getElementById('occupied-spaces').textContent = occ;
    document.getElementById('total-spaces').textContent = total;

    // 2. Update Overall Status
    const overallStatusEl = document.getElementById('overall-status');
    overallStatusEl.className = 'overall-status'; // Reset classes

    if (free > 0 || (data.parkingStatus === "AVAILABLE")) {
        overallStatusEl.textContent = "CAR PARK AVAILABLE";
        overallStatusEl.classList.add('available');
    } else if (free === 0 || (data.parkingStatus === "FULL")) {
        overallStatusEl.textContent = "CAR PARK FULL";
        overallStatusEl.classList.add('full');
    } else {
        overallStatusEl.textContent = "STATUS UNKNOWN";
        overallStatusEl.classList.add('default');
    }

    // 3. Update Individual Bays
    updateBayStatus('bay1', data.bay1?.status);
    updateBayStatus('bay2', data.bay2?.status);
    updateBayStatus('bay3', data.bay3?.status);
    updateBayStatus('bay4', data.bay4?.status);
}

function updateBayStatus(bayId, status) {
    const cardEl = document.getElementById(`${bayId}-card`);
    const statusEl = document.getElementById(`${bayId}-status`);

    if (!cardEl || !statusEl) return;

    // Clear existing status classes
    cardEl.classList.remove('available', 'occupied');

    if (status === 'AVAILABLE') {
        cardEl.classList.add('available');
        statusEl.textContent = 'AVAILABLE';
    } else if (status === 'OCCUPIED') {
        cardEl.classList.add('occupied');
        statusEl.textContent = 'OCCUPIED';
    } else {
        statusEl.textContent = 'UNKNOWN';
    }
}

function updateTimestamp() {
    const now = new Date();
    const timeString = now.toLocaleTimeString();
    document.getElementById('last-update-time').textContent = timeString;
}
