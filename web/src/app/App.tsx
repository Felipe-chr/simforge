import { Panel } from '../components/ui'
import { ComponentLibrary } from '../features/editor/ComponentLibrary'
import { EditorCanvas } from '../features/editor/EditorCanvas'
import { Inspector } from '../features/editor/Inspector'
import { SimControls } from '../features/simulation/SimControls'
import { WorkspaceProvider } from '../stores/workspace'
import { Dock } from './Dock'
import { StatusBar } from './StatusBar'
import { Toolbar } from './Toolbar'

export function App() {
  return (
    <WorkspaceProvider>
      <div className="flex h-full min-h-0 flex-col bg-shell-950">
        <Toolbar />

        <div className="flex min-h-0 flex-1">
          <Panel
            title="Componentes"
            className="w-60 shrink-0 border-r border-line"
            bodyClassName="min-h-0"
          >
            <ComponentLibrary />
          </Panel>

          <main className="flex min-w-0 flex-1 flex-col">
            <SimControls />
            <div className="min-h-0 flex-1">
              <EditorCanvas />
            </div>
            <Dock />
          </main>

          <Panel
            title="Inspetor"
            className="w-72 shrink-0 border-l border-line"
            bodyClassName="min-h-0"
          >
            <Inspector />
          </Panel>
        </div>

        <StatusBar />
      </div>
    </WorkspaceProvider>
  )
}
