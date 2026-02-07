import { useState } from 'react';
import FileExplorer from './components/FileExplorer';
import { useTheme } from './contexts/ThemeContext';

// Define the absolute paths used by your C++ Service
const SCOPES = {
  DOCUMENTS: 'C:/Users/Adi/Documents',
  DOWNLOADS: 'C:/Users/Adi/Downloads'
};

function App() {
  const { theme, toggleTheme } = useTheme();
  // Default to Documents view
  const [currentScope, setCurrentScope] = useState(SCOPES.DOCUMENTS);

  return (
    <div className={`flex min-h-screen transition-colors duration-500 ${theme === 'stealth' ? 'bg-gray-950 text-white' : 'bg-gray-50 text-gray-900'
      }`}>

      {/* Sidebar Navigation */}
      <aside className={`w-64 border-r p-6 flex flex-col gap-4 backdrop-blur-md bg-opacity-50 ${theme === 'stealth' ? 'border-white/10' : 'border-gray-200 bg-gray-100'
        }`}>
        <div className="mb-8">
          <h1 className="text-xl font-bold tracking-tighter">GATEKEEPER</h1>
          <p className="text-[10px] opacity-40 font-mono mt-1">v3.0_MULTI_SCOPE</p>
        </div>

        <nav className="flex flex-col gap-2">
          <button
            onClick={() => setCurrentScope(SCOPES.DOCUMENTS)}
            className={`flex items-center gap-3 px-4 py-3 rounded-lg transition-all text-sm font-medium ${currentScope === SCOPES.DOCUMENTS
              ? 'bg-blue-600/20 text-blue-500 border border-blue-600/50'
              : 'hover:bg-black/5 dark:hover:bg-white/5 opacity-60 hover:opacity-100'
              }`}
          >
            <span className="text-lg">📄</span> Documents
          </button>

          <button
            onClick={() => setCurrentScope(SCOPES.DOWNLOADS)}
            className={`flex items-center gap-3 px-4 py-3 rounded-lg transition-all text-sm font-medium ${currentScope === SCOPES.DOWNLOADS
              ? 'bg-blue-600/20 text-blue-500 border border-blue-600/50'
              : 'hover:bg-black/5 dark:hover:bg-white/5 opacity-60 hover:opacity-100'
              }`}
          >
            <span className="text-lg">📥</span> Downloads
          </button>
        </nav>

        <div className="mt-auto">
          <button
            onClick={toggleTheme}
            className="w-full text-xs border border-current opacity-30 py-2 rounded hover:opacity-100 transition-opacity"
          >
            {theme === 'stealth' ? 'ENABLE LIGHT_MODE' : 'ENABLE STEALTH_MODE'}
          </button>
        </div>
      </aside>

      {/* Main Content Area */}
      <main className="flex-1 flex flex-col h-screen overflow-hidden">
        <header className={`p-4 border-b flex justify-between items-center backdrop-blur-sm ${theme === 'stealth' ? 'border-white/10' : 'border-gray-200'
          }`}>
          <div className="flex items-center gap-2">
            <div className="w-2 h-2 rounded-full bg-green-500 animate-pulse"></div>
            <span className="text-xs font-mono opacity-50 uppercase tracking-widest">
              Location: {currentScope}
            </span>
          </div>
        </header>

        <section className="flex-1 p-6 overflow-auto">
          {/* The 'key' prop is critical here. When currentScope changes, 
            React will destroy the old FileExplorer and mount a new one,
            triggering a fresh Supabase fetch for the new directory.
          */}
          <FileExplorer key={currentScope} rootPath={currentScope} />
        </section>
      </main>
    </div>
  );
}

export default App;
