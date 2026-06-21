// Built-in JS apps (the Embedded app store — Plan 37 Fase 5).
// Each entry is the extracted JS bundle of a built .papp, installed at boot via
// loadEmbeddedJsApps() → JsAppStore::installApp(). Currently hand-maintained; a
// .papp-based regenerator is a follow-up (the old KAPP-based generator was removed).
#pragma once
namespace nema {
struct EmbeddedApp { const char* id; const char* name; const char* js; };
inline const EmbeddedApp EMBEDDED_APPS[] = {
    { "com.palanu.example.sysinfo", "Sys Info", R"KJS(
import{View as F,Text as z,Pressable as G,ScrollView as H,useState as I}from"nema";import{jsxDEV as f}from"nema/jsx-dev-runtime";function J(){let[A,B]=I(Number(nema.storage.get("taps")||"0")),C=()=>{let q=A+1;B(q),nema.storage.set("taps",String(q)),nema.log("info","SysInfo","tap "+q)};return f(F,{style:{flexDirection:"column",padding:3,gap:2},children:[f(z,{variant:"title",children:nema.device.name},void 0,!1,void 0,this),f(G,{onPress:C,children:f(z,{children:`Taps: ${A}  (tap +)`},void 0,!1,void 0,this)},void 0,!1,void 0,this),f(z,{variant:"caption",children:"Capabilities:"},void 0,!1,void 0,this),f(H,{style:{flexGrow:1},children:nema.device.caps.map((q)=>f(z,{children:"- "+q},void 0,!1,void 0,this))},void 0,!1,void 0,this)]},void 0,!0,void 0,this)}export{J as default};

)KJS" },
    { "com.palanu.example.counter", "Counter (JS)", R"KJS(
import{View as U,Text as B,Pressable as G,Row as W,useState as K,useEffect as M}from"nema";import{jsxDEV as q}from"nema/jsx-dev-runtime";var O="count.txt";function X(){let[C,A]=K(0),[H,Q]=K(!1);return M(()=>{let z=nema.storage.fs.readFile(O);if(z!==null){let J=parseInt(z,10);if(!isNaN(J))A(J)}Q(!0)},[]),M(()=>{if(!H)return;nema.storage.fs.writeFile(O,String(C))},[C,H]),q(U,{style:{flexDirection:"column",padding:4,gap:6,alignItems:"center"},children:[q(B,{variant:"title",children:`Count: ${C}`},void 0,!1,void 0,this),q(W,{style:{gap:8},children:[q(G,{onPress:()=>A((z)=>z-1),children:q(B,{children:"-"},void 0,!1,void 0,this)},void 0,!1,void 0,this),q(G,{onPress:()=>A((z)=>z+1),children:q(B,{children:"+"},void 0,!1,void 0,this)},void 0,!1,void 0,this)]},void 0,!0,void 0,this),q(G,{onPress:()=>A(0),children:q(B,{children:"Reset"},void 0,!1,void 0,this)},void 0,!1,void 0,this)]},void 0,!0,void 0,this)}export{X as default};

)KJS" },
};
inline constexpr int EMBEDDED_APP_COUNT = 2;
}
