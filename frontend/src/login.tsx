/* @refresh reload */
import { render } from 'solid-js/web';
import './index.css';
import { Component, createSignal } from 'solid-js';

const root = document.getElementById('root');

if (import.meta.env.DEV && !(root instanceof HTMLElement)) {
	throw new Error(
		'Root element not found. Did you forget to add it to your index.html? Or maybe the id attribute got misspelled?',
	);
}

const App : Component = () => {
	const [username, setUsername] = createSignal<string>("");
	const [password, setPassword] = createSignal<string>("");

	async function loginRequest(e: Event) {
		e.preventDefault();

		const loginResponse = await fetch('/api/login', {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify({ username: username(), password: password() })
		});

		// TODO: (Cesar) Display some info to the user (wrong username or password)
		if(loginResponse.ok) {
			// We want a refresh so we can GET the new root
			window.location.reload();
		}
		else {
			console.error(await loginResponse.text());
		}

		// TODO: (Cesar) Display warning when not using HTTPS
		// 				 The user should always enable HTTPS!
		// 				 Not doing so is broken and leaks user data
		// 				 Due to sending plain text password / usernames
		// 				 This is not critical if you don't care and
		// 				 are behind a VPN / proxy and trust other users
	}

	return (
		<div class="min-h-screen flex items-center justify-center bg-gray-100">
			<div class="w-full max-w-md bg-white rounded-2xl shadow-lg p-8">
				<div class="flex justify-center mb-6">
					<img src="/logo.png" class="w-full h-auto"/>
				</div>
				<h2 class="text-2xl font-semibold text-center mb-6">Login</h2>
				<form onSubmit={loginRequest} class="space-y-4">
					<div>
						<label class="block text-gray-700 text-sm font-medium mb-1">Username</label>
						<input
							type="text"
							value={username()}
							onInput={(e) => setUsername(e.currentTarget.value)}
							class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
							required
						/>
					</div>
					<div>
						<label class="block text-gray-700 text-sm font-medium mb-1">Password</label>
						<input
							type="password"
							value={password()}
							onInput={(e) => setPassword(e.currentTarget.value)}
							class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
							required
						/>
					</div>
					<button
						type="submit"
						class="w-full bg-blue-600 text-white py-2 rounded-lg hover:bg-blue-700 transition"
					>
						Log In
					</button>
				</form>
			</div>
		</div>
	);
};

render(() => <App/>, root!);
