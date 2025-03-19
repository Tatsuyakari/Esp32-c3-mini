// Theme switcher functionality
document.addEventListener('DOMContentLoaded', function() {
  // Check for saved theme preference or use preferred color scheme
  const savedTheme = localStorage.getItem('theme');
  const prefersDark = window.matchMedia('(prefers-color-scheme: dark)').matches;
  
  // Apply theme based on saved preference or system preference
  if (savedTheme === 'dark' || (!savedTheme && prefersDark)) {
    document.body.classList.add('dark-theme');
  }
  
  // Create theme toggle button
  const themeToggle = document.createElement('button');
  themeToggle.id = 'theme-toggle';
  themeToggle.className = 'theme-toggle';
  themeToggle.innerHTML = document.body.classList.contains('dark-theme') ? 
    '<i class="fas fa-sun"></i>' : 
    '<i class="fas fa-moon"></i>';
  
  // Add toggle button to the page
  document.querySelector('header').appendChild(themeToggle);
  
  // Add event listener to toggle theme
  themeToggle.addEventListener('click', function() {
    document.body.classList.toggle('dark-theme');
    
    // Update button icon
    this.innerHTML = document.body.classList.contains('dark-theme') ? 
      '<i class="fas fa-sun"></i>' : 
      '<i class="fas fa-moon"></i>';
    
    // Save preference to localStorage
    localStorage.setItem('theme', 
      document.body.classList.contains('dark-theme') ? 'dark' : 'light'
    );
  });
  
  // Add animation to cards
  const cards = document.querySelectorAll('.card');
  cards.forEach((card, index) => {
    card.style.animationDelay = `${index * 0.1}s`;
    card.classList.add('card-animate');
  });
});
