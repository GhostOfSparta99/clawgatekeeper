import FileExplorer from './components/FileExplorer';
import { useTheme } from './contexts/ThemeContext';

function App() {
  const { theme, toggleTheme } = useTheme();

  return (
    <div className={`min-h-screen transition-colors duration-500 ${theme === 'stealth' ? 'bg-gray-950 text-white' : 'bg-gray-50 text-gray-900'}`}>
      <header className="p-4 border-b border-white/10 sticky top-0 backdrop-blur-md z-10 w-full flex justify-between items-center">
        <h1 className="text-xl font-bold">GATEKEEPER</h1>
        <button onClick={toggleTheme} className="text-sm border px-3 py-1 rounded">
          {theme === 'stealth' ? 'Light Mode' : 'Stealth Mode'}
        </button>
      </header>

      <main className="p-4">
        <FileExplorer />
      </main>
    </div>
  );
}

export default App;
