const ROOM_COUNT=3;
const DEVICE_NAMES=["Iron Box","Kettle","Hair Dryer"];
function deviceName(id){const i=Number(String(id).replace("room",""))-1;return DEVICE_NAMES[i]||id;}
const FIRST_LIMIT=0.20, SECOND_LIMIT=0.75;
const powerHistory=Array.from({length:ROOM_COUNT},()=>[]);
const timeHistory=[];
let powerChart=null;
let busy=false;
function el(id){return document.getElementById(id)}
function txt(id,value){const e=el(id);if(e)e.textContent=value}
function num(v){const n=Number(v);return Number.isFinite(n)?n:0}
function initChart(){
 const canvas=el('powerChart');
 if(!canvas||typeof Chart==='undefined')return;
 powerChart=new Chart(canvas,{type:'line',data:{labels:timeHistory,datasets:Array.from({length:ROOM_COUNT},(_,i)=>({label:DEVICE_NAMES[i],data:powerHistory[i],borderWidth:2,tension:.3,fill:false}))},options:{responsive:true,maintainAspectRatio:false,animation:false}});
}
function updateRoom(id,r){
 const n=id.replace('room','');
 txt(id+'Power',num(r.power).toFixed(1)+' W');
 txt(id+'Voltage',num(r.voltage).toFixed(1)+' V');
 txt(id+'Current',num(r.current).toFixed(3)+' A');
 txt(id+'Energy',num(r.energy).toFixed(3)+' kWh');
 txt(id+'Frequency',num(r.frequency).toFixed(1)+' Hz');
 txt(id+'PF',num(r.pf).toFixed(2));
 txt(id+'Limit',`${FIRST_LIMIT} / ${SECOND_LIMIT} kWh`);
 const bar=el(id+'PowerBar');if(bar)bar.style.width=Math.min(100,num(r.energy)/SECOND_LIMIT*100)+'%';
 const status=el(id+'Status');if(status){const off=num(r.trip)===1||num(r.relay)===0;status.textContent=off?'TRIPPED':num(r.pending)===1?'WAITING FOR USER':num(r.communication)===0?'OFFLINE':'ACTIVE';}
}
function renderAlerts(data){
 const pending=Array.from({length:ROOM_COUNT},(_,i)=>({id:'room'+(i+1),r:data['room'+(i+1)]||{}})).filter(x=>num(x.r.pending)===1);
 txt('notificationCount',pending.length);txt('pendingAlerts',pending.length);
 const list=el('notificationList');if(!list)return;
 list.replaceChildren();
 if(!pending.length){list.innerHTML='<div class="empty-notification"><p>No pending energy alerts.</p></div>';return;}
 pending.forEach(({id,r})=>{
 const card=document.createElement('div');card.className='notification-card';
 const title=document.createElement('div');title.className='notification-title';title.textContent=`${deviceName(id)} - ${r.reason==='SECOND_ENERGY_LIMIT_EXCEEDED'?'Second energy warning':'Energy warning'}`;
 const info=document.createElement('p');info.textContent=`Energy: ${num(r.energy).toFixed(3)} kWh | Threshold: ${num(r.energy_limit)||FIRST_LIMIT} kWh | Power: ${num(r.power).toFixed(1)} W. Relay changes only after your decision.`;
 const actions=document.createElement('div');actions.className='notification-actions';
 for(const [action,label,cls] of [['allow','ALLOW TRIP','allow-trip-btn'],['deny',"DON'T TRIP",'deny-trip-btn']]){
  const btn=document.createElement('button');btn.className=cls;btn.textContent=label;btn.onclick=()=>sendTripDecision(id,action);actions.appendChild(btn);
 }
 card.append(title,info,actions);list.appendChild(card);
 });
}
async function sendTripDecision(room,action){
 try{const res=await fetch('/api/trip-decision',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({room,action})});if(!res.ok)throw new Error('Decision failed');await loadDashboard();}
 catch(e){alert(e.message)}
}
async function loadDashboard(){
 if(busy)return;busy=true;
 try{
  const response=await fetch('/api/data',{cache:'no-store'});if(!response.ok)throw Error('Dashboard offline');const data=await response.json();
  let power=0,energy=0,active=0;
  const values=[];
  for(let i=1;i<=ROOM_COUNT;i++){
   const id='room'+i,r=data[id]||{};updateRoom(id,r);power+=num(r.power);energy+=num(r.energy);if(num(r.relay)===1)active++;values.push(num(r.power));
  }
  txt('totalPower',power.toFixed(1)+' W');txt('totalEnergy',energy.toFixed(3)+' kWh');txt('activeRooms',active);
  txt('lastUpdate',data.last_update||'Never');renderAlerts(data);
  timeHistory.push(new Date().toLocaleTimeString());values.forEach((v,i)=>powerHistory[i].push(v));
  if(timeHistory.length>30){timeHistory.shift();powerHistory.forEach(a=>a.shift())}if(powerChart)powerChart.update('none');
  const c=el('connectionStatus');if(c){c.textContent=data.esp32_connected?'Connected':'Disconnected';c.style.color=data.esp32_connected?'#16a34a':'#dc2626';}
 }catch(e){console.error(e);const c=el('connectionStatus');if(c)c.textContent='Disconnected'}finally{busy=false}
}
function scrollToNotifications(){el('notificationList')?.scrollIntoView({behavior:'smooth'})}
document.addEventListener('DOMContentLoaded',()=>{initChart();loadDashboard();setInterval(loadDashboard,2000)});
