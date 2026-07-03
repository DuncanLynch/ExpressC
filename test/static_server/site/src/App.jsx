import { BrowserRouter, Routes, Route } from 'react-router-dom'
import Nav from './components/Nav'
import Hero from './sections/Hero'
import Features from './sections/Features'
import GettingStarted from './sections/GettingStarted'
import APIReference from './sections/APIReference'
import Demo from './sections/Demo'
import Footer from './components/Footer'
import './App.css'

function Docs() {
  return (
    <>
      <Nav />
      <main>
        <Hero />
        <Features />
        <GettingStarted />
        <APIReference />
        <Demo />
      </main>
      <Footer />
    </>
  )
}

export default function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route path="*" element={<Docs />} />
      </Routes>
    </BrowserRouter>
  )
}
